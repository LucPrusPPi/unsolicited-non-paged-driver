#include <unpd/common.h>
#include <unpd/mmu/physical_memory.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

NTSTATUS PhysicalMemory::ReadPhysicalAddress(ULONG64 physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead) {
    if (!physicalAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsPhysicalAddressAllowed(physicalAddress, size)) {
        return STATUS_ACCESS_DENIED;
    }

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
}

NTSTATUS PhysicalMemory::WritePhysicalAddress(ULONG64 physicalAddress, const void* buffer, SIZE_T size, PSIZE_T bytesWritten) {
    if (!physicalAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!IsPhysicalAddressAllowed(physicalAddress, size)) {
        return STATUS_ACCESS_DENIED;
    }

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
}

#endif // UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

} // namespace unpd::mmu

