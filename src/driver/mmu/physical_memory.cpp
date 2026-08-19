#include <unpd/common.h>
#include <unpd/mmu/physical_memory.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

NTSTATUS PhysicalMemory::ReadPhysicalAddress(ULONG64 physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead) {
    if (!physicalAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    if (bytesRead) *bytesRead = size;
    return STATUS_SUCCESS;
}

NTSTATUS PhysicalMemory::WritePhysicalAddress(ULONG64 physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten) {
    if (!physicalAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    if (bytesWritten) *bytesWritten = size;
    return STATUS_SUCCESS;
}

#endif // UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

} // namespace unpd::mmu
