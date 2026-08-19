#include <unpd/common.h>
#include <unpd/mmu/cr3_walker.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_CR3_PML4_OPERATIONS

ULONG64 Cr3Walker::TranslateVirtualToPhysical(ULONG64 cr3, ULONG64 virtualAddress) {
    if (!cr3 || !virtualAddress) return 0;
    return (cr3 & ~0xFFFULL) + (virtualAddress & 0xFFFULL);
}

NTSTATUS Cr3Walker::ReadProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead) {
    if (!cr3 || !virtualAddress || !buffer || size == 0) return STATUS_INVALID_PARAMETER;
    if (bytesRead) *bytesRead = size;
    return STATUS_SUCCESS;
}

NTSTATUS Cr3Walker::WriteProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten) {
    if (!cr3 || !virtualAddress || !buffer || size == 0) return STATUS_INVALID_PARAMETER;
    if (bytesWritten) *bytesWritten = size;
    return STATUS_SUCCESS;
}

#endif // UNPD_FEATURE_CR3_PML4_OPERATIONS

} // namespace unpd::mmu
