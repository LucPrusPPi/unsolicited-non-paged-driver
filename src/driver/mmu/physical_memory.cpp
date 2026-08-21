#include <unpd/common.h>
#include <unpd/mmu/physical_memory.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

#ifdef _KERNEL_MODE
static NTSTATUS SafeCopyBuffer(void* dest, const void* src, SIZE_T size) noexcept {
    __try {
        RtlCopyMemory(dest, src, size);
        return STATUS_SUCCESS;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
}
#endif

NTSTATUS PhysicalMemory::ReadPhysicalAddress(ULONG64 physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead) {
    if (!physicalAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsPhysicalAddressAllowed(physicalAddress, size)) {
        return STATUS_ACCESS_DENIED;
    }

#ifdef _KERNEL_MODE
    MM_COPY_ADDRESS sourceAddress{};
    sourceAddress.PhysicalAddress.QuadPart = static_cast<LONGLONG>(physicalAddress);
    SIZE_T copied = 0;

    NTSTATUS status = MmCopyMemory(
        buffer,
        sourceAddress,
        size,
        MM_COPY_MEMORY_PHYSICAL,
        &copied
    );

    if (bytesRead) *bytesRead = copied;
    return status;
#else
    // Usermode Mock
    SIZE_T totalRead = 0;
    auto* dest = static_cast<uint8_t*>(buffer);

    while (totalRead < size) {
        const ULONG64 currentPa = physicalAddress + totalRead;
        const SIZE_T pageOffset = currentPa & 0xFFFULL;
        const SIZE_T chunk = (size - totalRead < (4096ULL - pageOffset)) ? (size - totalRead) : (4096ULL - pageOffset);

        PhysicalMemoryMapping<uint8_t> mapping(currentPa, chunk);
        if (!mapping.IsValid()) {
            if (bytesRead) *bytesRead = totalRead;
            return STATUS_UNSUCCESSFUL;
        }

        memcpy(dest + totalRead, mapping.Get(), chunk);
        totalRead += chunk;
    }

    if (bytesRead) *bytesRead = totalRead;
    return STATUS_SUCCESS;
#endif
}

NTSTATUS PhysicalMemory::WritePhysicalAddress(ULONG64 physicalAddress, const void* buffer, SIZE_T size, PSIZE_T bytesWritten) {
    if (!physicalAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsPhysicalAddressAllowed(physicalAddress, size)) {
        return STATUS_ACCESS_DENIED;
    }

#ifdef _KERNEL_MODE
    SIZE_T totalWritten = 0;
    const auto* src = static_cast<const uint8_t*>(buffer);

    while (totalWritten < size) {
        const ULONG64 currentPa = physicalAddress + totalWritten;
        const SIZE_T pageOffset = currentPa & 0xFFFULL;
        const SIZE_T chunk = (size - totalWritten < (4096ULL - pageOffset)) ? (size - totalWritten) : (4096ULL - pageOffset);

        PhysicalMemoryMapping<uint8_t> mapping(currentPa, chunk);
        if (!mapping.IsValid()) {
            if (bytesWritten) *bytesWritten = totalWritten;
            return STATUS_UNSUCCESSFUL;
        }

        NTSTATUS copyStatus = SafeCopyBuffer(mapping.Get(), src + totalWritten, chunk);
        if (!NT_SUCCESS(copyStatus)) {
            if (bytesWritten) *bytesWritten = totalWritten;
            return copyStatus;
        }

        totalWritten += chunk;
    }

    if (bytesWritten) *bytesWritten = totalWritten;
    return STATUS_SUCCESS;
#else
    // Usermode Mock
    SIZE_T totalWritten = 0;
    const auto* src = static_cast<const uint8_t*>(buffer);

    while (totalWritten < size) {
        const ULONG64 currentPa = physicalAddress + totalWritten;
        const SIZE_T pageOffset = currentPa & 0xFFFULL;
        const SIZE_T chunk = (size - totalWritten < (4096ULL - pageOffset)) ? (size - totalWritten) : (4096ULL - pageOffset);

        PhysicalMemoryMapping<uint8_t> mapping(currentPa, chunk);
        if (!mapping.IsValid()) {
            if (bytesWritten) *bytesWritten = totalWritten;
            return STATUS_UNSUCCESSFUL;
        }

        memcpy(mapping.Get(), src + totalWritten, chunk);
        totalWritten += chunk;
    }

    if (bytesWritten) *bytesWritten = totalWritten;
    return STATUS_SUCCESS;
#endif
}

#endif // UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

} // namespace unpd::mmu

