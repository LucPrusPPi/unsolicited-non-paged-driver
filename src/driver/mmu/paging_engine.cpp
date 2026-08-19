#include "unpd/mmu/paging_engine.hpp"
#include "unpd/mmu/physical_memory.hpp"

#ifdef _KERNEL_MODE

namespace unpd::mmu {

static NTSTATUS SafeProbeAndRead(const void* sourceAddress, void* destination, SIZE_T size) noexcept {
    __try {
        ProbeForRead(const_cast<void*>(sourceAddress), size, 1);
        RtlCopyMemory(destination, sourceAddress, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
    return STATUS_SUCCESS;
}

static NTSTATUS SafeProbeAndWrite(void* targetAddress, const void* source, SIZE_T size) noexcept {
    __try {
        ProbeForWrite(targetAddress, size, 1);
        RtlCopyMemory(targetAddress, source, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
    return STATUS_SUCCESS;
}

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

    uintptr_t vaInt = reinterpret_cast<uintptr_t>(virtualAddress);

    // Canonical 48-bit address validation
    uintptr_t signExtension = vaInt >> 47;
    if (signExtension != 0 && signExtension != 0x1FFFF) {
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
        res.IsUserAccessible = (vaInt < 0x7FFFFFFFFFFFULL);
        res.IsExecutable = true;
        return res;
    }

    VIRTUAL_ADDRESS_64 va{};
    va.Pointer = const_cast<void*>(virtualAddress);

    CR3_REGISTER_64 cr3{};
    cr3.Value = cr3Value;

    const ULONG64 pml4Base = cr3.Pml4PhysicalAddress << PAGE_SHIFT_4KB;
    const ULONG64 pml4Index = va.Pml4Index;
    ULONG64 pml4e = 0;
    SIZE_T bytesRead = 0;
    if (PhysicalMemory::ReadPhysicalAddress(pml4Base + (pml4Index * 8), &pml4e, sizeof(pml4e), &bytesRead) != STATUS_SUCCESS || !(pml4e & 1ULL)) {
        return kstd::expected<TranslationResult>::error(STATUS_UNSUCCESSFUL);
    }

    const ULONG64 pdptBase = pml4e & 0x000FFFFFFFFFF000ULL;
    const ULONG64 pdptIndex = va.PdptIndex;
    ULONG64 pdpte = 0;
    if (PhysicalMemory::ReadPhysicalAddress(pdptBase + (pdptIndex * 8), &pdpte, sizeof(pdpte), &bytesRead) != STATUS_SUCCESS || !(pdpte & 1ULL)) {
        return kstd::expected<TranslationResult>::error(STATUS_UNSUCCESSFUL);
    }

    TranslationResult res{};
    res.IsUserAccessible = (va.Pml4Index < 256);

    // 1GB Huge Page Check
    if (pdpte & (1ULL << 7)) {
        res.PhysicalAddress = (pdpte & 0x000FFFFFC0000000ULL) + (vaInt & 0x3FFFFFFFULL);
        res.PageSize = PAGE_SIZE_1GB;
        res.IsPresent = true;
        res.IsWritable = (pdpte & (1ULL << 1)) != 0;
        res.IsExecutable = (pdpte & (1ULL << 63)) == 0;
        return res;
    }

    const ULONG64 pdBase = pdpte & 0x000FFFFFFFFFF000ULL;
    const ULONG64 pdIndex = va.PdIndex;
    ULONG64 pde = 0;
    if (PhysicalMemory::ReadPhysicalAddress(pdBase + (pdIndex * 8), &pde, sizeof(pde), &bytesRead) != STATUS_SUCCESS || !(pde & 1ULL)) {
        return kstd::expected<TranslationResult>::error(STATUS_UNSUCCESSFUL);
    }

    // 2MB Large Page Check
    if (pde & (1ULL << 7)) {
        res.PhysicalAddress = (pde & 0x000FFFFFFFE00000ULL) + (vaInt & 0x1FFFFFULL);
        res.PageSize = PAGE_SIZE_2MB;
        res.IsPresent = true;
        res.IsWritable = (pde & (1ULL << 1)) != 0;
        res.IsExecutable = (pde & (1ULL << 63)) == 0;
        return res;
    }

    const ULONG64 ptBase = pde & 0x000FFFFFFFFFF000ULL;
    const ULONG64 ptIndex = va.PtIndex;
    ULONG64 pte = 0;
    if (PhysicalMemory::ReadPhysicalAddress(ptBase + (ptIndex * 8), &pte, sizeof(pte), &bytesRead) != STATUS_SUCCESS || !(pte & 1ULL)) {
        return kstd::expected<TranslationResult>::error(STATUS_UNSUCCESSFUL);
    }

    res.PhysicalAddress = (pte & 0x000FFFFFFFFFF000ULL) + va.Offset4KB;
    res.PageSize = PAGE_SIZE_4KB;
    res.IsPresent = true;
    res.IsWritable = (pte & (1ULL << 1)) != 0;
    res.IsExecutable = (pte & (1ULL << 63)) == 0;

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
        status = SafeProbeAndRead(sourceAddress, intermediate, size);
    }

    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(intermediate, 'MPNU');
        return status;
    }

    // Step 2: Write from intermediate kernel buffer to target process
    {
        ProcessAttachmentGuard attachTarget(targetProcess);
        status = SafeProbeAndWrite(targetAddress, intermediate, size);
    }

    ExFreePoolWithTag(intermediate, 'MPNU');
    return status;
}

} // namespace unpd::mmu

#endif // _KERNEL_MODE
