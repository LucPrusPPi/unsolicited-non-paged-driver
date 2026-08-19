#include <unpd/common.h>
#include <unpd/mmu/cr3_walker.hpp>
#include <unpd/mmu/physical_memory.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_CR3_PML4_OPERATIONS

ULONG64 Cr3Walker::TranslateVirtualToPhysical(ULONG64 cr3, ULONG64 virtualAddress) {
    if (!cr3 || !virtualAddress) {
        return 0;
    }

#ifndef _KERNEL_MODE
    // Deterministic mock algorithm for usermode testing
    return (cr3 & ~0xFFFULL) + (virtualAddress & 0xFFFULL);
#else
    const ULONG64 pml4Base = cr3 & 0x000FFFFFFFFFF000ULL;
    const ULONG64 pml4Index = (virtualAddress >> 39) & 0x1FFULL;
    const ULONG64 pml4eAddress = pml4Base + (pml4Index * sizeof(ULONG64));

    ULONG64 pml4e = 0;
    SIZE_T bytesRead = 0;
    if (PhysicalMemory::ReadPhysicalAddress(pml4eAddress, &pml4e, sizeof(pml4e), &bytesRead) != STATUS_SUCCESS || !(pml4e & 1ULL)) {
        return 0; // PML4E not present
    }

    const ULONG64 pdptBase = pml4e & 0x000FFFFFFFFFF000ULL;
    const ULONG64 pdptIndex = (virtualAddress >> 30) & 0x1FFULL;
    const ULONG64 pdpteAddress = pdptBase + (pdptIndex * sizeof(ULONG64));

    ULONG64 pdpte = 0;
    if (PhysicalMemory::ReadPhysicalAddress(pdpteAddress, &pdpte, sizeof(pdpte), &bytesRead) != STATUS_SUCCESS || !(pdpte & 1ULL)) {
        return 0; // PDPTE not present
    }

    // 1GB Large Page (bit 7 set)
    if (pdpte & (1ULL << 7)) {
        return (pdpte & 0x000FFFFFC0000000ULL) + (virtualAddress & 0x3FFFFFFFULL);
    }

    const ULONG64 pdBase = pdpte & 0x000FFFFFFFFFF000ULL;
    const ULONG64 pdIndex = (virtualAddress >> 21) & 0x1FFULL;
    const ULONG64 pdeAddress = pdBase + (pdIndex * sizeof(ULONG64));

    ULONG64 pde = 0;
    if (PhysicalMemory::ReadPhysicalAddress(pdeAddress, &pde, sizeof(pde), &bytesRead) != STATUS_SUCCESS || !(pde & 1ULL)) {
        return 0; // PDE not present
    }

    // 2MB Large Page (bit 7 set)
    if (pde & (1ULL << 7)) {
        return (pde & 0x000FFFFFFFE00000ULL) + (virtualAddress & 0x1FFFFFULL);
    }

    const ULONG64 ptBase = pde & 0x000FFFFFFFFFF000ULL;
    const ULONG64 ptIndex = (virtualAddress >> 12) & 0x1FFULL;
    const ULONG64 pteAddress = ptBase + (ptIndex * sizeof(ULONG64));

    ULONG64 pte = 0;
    if (PhysicalMemory::ReadPhysicalAddress(pteAddress, &pte, sizeof(pte), &bytesRead) != STATUS_SUCCESS || !(pte & 1ULL)) {
        return 0; // PTE not present
    }

    // Standard 4KB Page
    return (pte & 0x000FFFFFFFFFF000ULL) + (virtualAddress & 0xFFFULL);
#endif
}

NTSTATUS Cr3Walker::ReadProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead) {
    if (!cr3 || !virtualAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T totalRead = 0;
    auto* dest = static_cast<uint8_t*>(buffer);

    while (totalRead < size) {
        const ULONG64 currentVa = virtualAddress + totalRead;
        const ULONG64 physicalAddress = TranslateVirtualToPhysical(cr3, currentVa);
        if (!physicalAddress) {
            if (bytesRead) *bytesRead = totalRead;
            return totalRead > 0 ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        }

        const SIZE_T pageOffset = currentVa & 0xFFFULL;
        const SIZE_T chunk = (size - totalRead < (4096ULL - pageOffset)) ? (size - totalRead) : (4096ULL - pageOffset);

        SIZE_T chunkRead = 0;
        const NTSTATUS status = PhysicalMemory::ReadPhysicalAddress(physicalAddress, dest + totalRead, chunk, &chunkRead);
        if (status != STATUS_SUCCESS || chunkRead == 0) {
            if (bytesRead) *bytesRead = totalRead;
            return totalRead > 0 ? STATUS_SUCCESS : status;
        }

        totalRead += chunkRead;
    }

    if (bytesRead) *bytesRead = totalRead;
    return STATUS_SUCCESS;
}

NTSTATUS Cr3Walker::WriteProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, const void* buffer, SIZE_T size, PSIZE_T bytesWritten) {
    if (!cr3 || !virtualAddress || !buffer || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T totalWritten = 0;
    const auto* src = static_cast<const uint8_t*>(buffer);

    while (totalWritten < size) {
        const ULONG64 currentVa = virtualAddress + totalWritten;
        const ULONG64 physicalAddress = TranslateVirtualToPhysical(cr3, currentVa);
        if (!physicalAddress) {
            if (bytesWritten) *bytesWritten = totalWritten;
            return totalWritten > 0 ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        }

        const SIZE_T pageOffset = currentVa & 0xFFFULL;
        const SIZE_T chunk = (size - totalWritten < (4096ULL - pageOffset)) ? (size - totalWritten) : (4096ULL - pageOffset);

        SIZE_T chunkWritten = 0;
        const NTSTATUS status = PhysicalMemory::WritePhysicalAddress(physicalAddress, src + totalWritten, chunk, &chunkWritten);
        if (status != STATUS_SUCCESS || chunkWritten == 0) {
            if (bytesWritten) *bytesWritten = totalWritten;
            return totalWritten > 0 ? STATUS_SUCCESS : status;
        }

        totalWritten += chunkWritten;
    }

    if (bytesWritten) *bytesWritten = totalWritten;
    return STATUS_SUCCESS;
}

#endif // UNPD_FEATURE_CR3_PML4_OPERATIONS

} // namespace unpd::mmu

