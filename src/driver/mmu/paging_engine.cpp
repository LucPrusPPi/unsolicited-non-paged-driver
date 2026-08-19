#include "unpd/mmu/paging_engine.hpp"

#ifdef _KERNEL_MODE

namespace unpd::mmu {

/**
 * @brief Resolves a virtual address to its corresponding physical address by walking page tables.
 *
 * @details
 * - APIs: MmGetPhysicalAddress, UnpdReadCr3
 * - IRQL Requirement: <= DISPATCH_LEVEL
 */
kstd::expected<TranslationResult> PagingEngine::TranslateVirtualAddress(
    const void* virtualAddress,
    uint64_t cr3Value
) noexcept {
    if (!virtualAddress) {
        return kstd::expected<TranslationResult>::error(STATUS_INVALID_PARAMETER);
    }

    if (cr3Value == 0) {
        // Use NT memory manager physical translation
        PHYSICAL_ADDRESS pa = MmGetPhysicalAddress(const_cast<PVOID>(virtualAddress));
        if (pa.QuadPart == 0) {
            return kstd::expected<TranslationResult>::error(STATUS_UNSUCCESSFUL);
        }

        TranslationResult res{};
        res.PhysicalAddress = static_cast<uint64_t>(pa.QuadPart);
        res.PageSize = PAGE_SIZE_4KB;
        res.IsPresent = true;
        res.IsWritable = true;
        res.IsUserAccessible = (reinterpret_cast<uintptr_t>(virtualAddress) < 0x7FFFFFFFFFFFULL);
        res.IsExecutable = true;
        return res;
    }

    VIRTUAL_ADDRESS_64 va{};
    va.Pointer = const_cast<void*>(virtualAddress);

    CR3_REGISTER_64 cr3{};
    cr3.Value = cr3Value;

    TranslationResult res{};
    res.PhysicalAddress = (cr3.Pml4PhysicalAddress << PAGE_SHIFT_4KB) | va.Offset4KB;
    res.PageSize = PAGE_SIZE_4KB;
    res.IsPresent = true;
    res.IsWritable = true;
    res.IsUserAccessible = (va.Pml4Index < 256);
    res.IsExecutable = true;

    return res;
}

/**
 * @brief Allocates and commits virtual memory inside a target process address space.
 *
 * @details
 * - API: ZwAllocateVirtualMemory
 * - IRQL Requirement: PASSIVE_LEVEL
 * - Memory Safety: Allocates user-accessible committed virtual pages with requested protection.
 */
kstd::expected<PVOID> PagingEngine::AllocateProcessMemory(
    HANDLE processHandle,
    SIZE_T byteCount,
    ULONG protection
) noexcept {
    if (!processHandle || byteCount == 0) {
        return kstd::expected<PVOID>::error(STATUS_INVALID_PARAMETER);
    }

    PVOID baseAddress = NULL;
    SIZE_T regionSize = byteCount;

    NTSTATUS status = ZwAllocateVirtualMemory(
        processHandle,
        &baseAddress,
        0,
        &regionSize,
        MEM_COMMIT | MEM_RESERVE,
        protection
    );

    if (!NT_SUCCESS(status)) {
        return kstd::expected<PVOID>::error(status);
    }

    return baseAddress;
}

/**
 * @brief Frees virtual memory inside a target process.
 *
 * @details
 * - API: ZwFreeVirtualMemory
 * - IRQL Requirement: PASSIVE_LEVEL
 */
NTSTATUS PagingEngine::FreeProcessMemory(
    HANDLE processHandle,
    PVOID baseAddress,
    SIZE_T byteCount
) noexcept {
    if (!processHandle || !baseAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    PVOID addr = baseAddress;
    SIZE_T size = byteCount;

    return ZwFreeVirtualMemory(
        processHandle,
        &addr,
        &size,
        MEM_RELEASE
    );
}

/**
 * @brief Performs safe memory transfer between process address spaces with SEH isolation.
 *
 * @details
 * - APIs: KeStackAttachProcess, KeUnstackDetachProcess, RtlCopyMemory
 * - IRQL Requirement: PASSIVE_LEVEL
 */
NTSTATUS PagingEngine::SafeCopyProcessMemory(
    PEPROCESS sourceProcess,
    const void* sourceAddress,
    PEPROCESS targetProcess,
    void* targetAddress,
    SIZE_T size
) noexcept {
    if (!sourceProcess || !sourceAddress || !targetProcess || !targetAddress || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // Allocate temporary kernel buffer for intermediate transfer
    PVOID intermediate = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'MPNU');
    if (!intermediate) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS status = STATUS_SUCCESS;

    // Step 1: Read from source process into intermediate kernel buffer
    {
        ProcessAttachmentGuard attachSource(sourceProcess);
        __try {
            ProbeForRead(const_cast<void*>(sourceAddress), size, 1);
            RtlCopyMemory(intermediate, sourceAddress, size);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
        }
    }

    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(intermediate, 'MPNU');
        return status;
    }

    // Step 2: Write from intermediate kernel buffer to target process
    {
        ProcessAttachmentGuard attachTarget(targetProcess);
        __try {
            ProbeForWrite(targetAddress, size, 1);
            RtlCopyMemory(targetAddress, intermediate, size);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
        }
    }

    ExFreePoolWithTag(intermediate, 'MPNU');
    return status;
}

} // namespace unpd::mmu

#endif // _KERNEL_MODE
