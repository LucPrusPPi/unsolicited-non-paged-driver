#pragma once

#ifndef UNPD_MMU_PTE_REMAPPER_HPP
#define UNPD_MMU_PTE_REMAPPER_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>
#include <unpd/mmu/physical_memory.hpp>
#include <unpd/kernel_asm.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_PTE_REMAPPER

class PteRemapper {
public:
    static ULONG64 GetPtePhysicalAddress(ULONG64 cr3, ULONG64 virtualAddress) {
        if (!cr3 || !virtualAddress) return 0;

#ifndef _KERNEL_MODE
        return (cr3 & ~0xFFFULL) + 0x100;
#else
        const ULONG64 pml4Base = cr3 & 0x000FFFFFFFFFF000ULL;
        const ULONG64 pml4Index = (virtualAddress >> 39) & 0x1FFULL;
        ULONG64 pml4e = 0;
        SIZE_T read = 0;
        if (PhysicalMemory::ReadPhysicalAddress(pml4Base + (pml4Index * 8), &pml4e, sizeof(pml4e), &read) != STATUS_SUCCESS || !(pml4e & 1ULL))
            return 0;

        const ULONG64 pdptBase = pml4e & 0x000FFFFFFFFFF000ULL;
        const ULONG64 pdptIndex = (virtualAddress >> 30) & 0x1FFULL;
        ULONG64 pdpte = 0;
        if (PhysicalMemory::ReadPhysicalAddress(pdptBase + (pdptIndex * 8), &pdpte, sizeof(pdpte), &read) != STATUS_SUCCESS || !(pdpte & 1ULL))
            return 0;
        if (pdpte & (1ULL << 7)) return pdptBase + (pdptIndex * 8);

        const ULONG64 pdBase = pdpte & 0x000FFFFFFFFFF000ULL;
        const ULONG64 pdIndex = (virtualAddress >> 21) & 0x1FFULL;
        ULONG64 pde = 0;
        if (PhysicalMemory::ReadPhysicalAddress(pdBase + (pdIndex * 8), &pde, sizeof(pde), &read) != STATUS_SUCCESS || !(pde & 1ULL))
            return 0;
        if (pde & (1ULL << 7)) return pdBase + (pdIndex * 8);

        const ULONG64 ptBase = pde & 0x000FFFFFFFFFF000ULL;
        const ULONG64 ptIndex = (virtualAddress >> 12) & 0x1FFULL;
        return ptBase + (ptIndex * 8);
#endif
    }

    static NTSTATUS MakePageWritable(ULONG64 cr3, ULONG64 virtualAddress) {
        if (!cr3 || !virtualAddress) {
            return STATUS_INVALID_PARAMETER;
        }

        const ULONG64 ptePa = GetPtePhysicalAddress(cr3, virtualAddress);
        if (!ptePa) {
            return STATUS_UNSUCCESSFUL;
        }

        ULONG64 pteValue = 0;
        SIZE_T ioBytes = 0;
        if (PhysicalMemory::ReadPhysicalAddress(ptePa, &pteValue, sizeof(pteValue), &ioBytes) != STATUS_SUCCESS) {
            return STATUS_UNSUCCESSFUL;
        }

        pteValue |= (1ULL << 1); // Set Read/Write bit

        if (PhysicalMemory::WritePhysicalAddress(ptePa, &pteValue, sizeof(pteValue), &ioBytes) != STATUS_SUCCESS) {
            return STATUS_UNSUCCESSFUL;
        }

#ifdef _KERNEL_MODE
        UnpdInvlpg(reinterpret_cast<const void*>(virtualAddress));
#endif
        return STATUS_SUCCESS;
    }

    static NTSTATUS MakePageExecutable(ULONG64 cr3, ULONG64 virtualAddress) {
        if (!cr3 || !virtualAddress) {
            return STATUS_INVALID_PARAMETER;
        }

        const ULONG64 ptePa = GetPtePhysicalAddress(cr3, virtualAddress);
        if (!ptePa) {
            return STATUS_UNSUCCESSFUL;
        }

        ULONG64 pteValue = 0;
        SIZE_T ioBytes = 0;
        if (PhysicalMemory::ReadPhysicalAddress(ptePa, &pteValue, sizeof(pteValue), &ioBytes) != STATUS_SUCCESS) {
            return STATUS_UNSUCCESSFUL;
        }

        pteValue &= ~(1ULL << 63); // Clear No-Execute (NX) bit

        if (PhysicalMemory::WritePhysicalAddress(ptePa, &pteValue, sizeof(pteValue), &ioBytes) != STATUS_SUCCESS) {
            return STATUS_UNSUCCESSFUL;
        }

#ifdef _KERNEL_MODE
        UnpdInvlpg(reinterpret_cast<const void*>(virtualAddress));
#endif
        return STATUS_SUCCESS;
    }
};

#else

class PteRemapper {
public:
    static NTSTATUS MakePageWritable(ULONG64, ULONG64) { return STATUS_NOT_SUPPORTED; }
    static NTSTATUS MakePageExecutable(ULONG64, ULONG64) { return STATUS_NOT_SUPPORTED; }
};

#endif // UNPD_FEATURE_PTE_REMAPPER

} // namespace unpd::mmu

#endif // UNPD_MMU_PTE_REMAPPER_HPP
