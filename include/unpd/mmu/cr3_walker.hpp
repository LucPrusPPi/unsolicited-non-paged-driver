#pragma once

#ifndef UNPD_MMU_CR3_WALKER_HPP
#define UNPD_MMU_CR3_WALKER_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>
#include <unpd/mmu/paging_types.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_CR3_PML4_OPERATIONS

/**
 * @brief Lockless 4-Level x86-64 Software Page Table (PML4) Traversal Engine.
 *
 * @details
 * Performs manual address translation by walking PML4 -> PDPTE -> PDE -> PTE tables.
 *
 * @note Architecture & KVAS Notes:
 * 1. Under KVA Shadowing (KPTI/Meltdown mitigations), ensure the passed CR3 is the
 *    Kernel DirectoryTableBase (`EPROCESS->Pcb.DirectoryTableBase[0]`), not UserDirectoryTableBase.
 * 2. PCID bits (0..11) are stripped automatically during traversal.
 * 3. Lockless traversal does not hold `MiLockPageTable` / Working Set lock. If a page
 *    is concurrently freed or in Transition/Standby state (Present bit = 0), traversal safely
 *    aborts without raising page faults, and physical access is performed via `MmCopyMemory`.
 */
class Cr3Walker {
public:
    static bool IsCanonical(ULONG64 virtualAddress) noexcept {
        const ULONG64 signBits = virtualAddress >> 47;
        return (signBits == 0) || (signBits == 0x1FFFFULL);
    }
    static ULONG64 TranslateVirtualToPhysical(ULONG64 cr3, ULONG64 virtualAddress);
    static NTSTATUS ReadProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead);
    static NTSTATUS WriteProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, const void* buffer, SIZE_T size, PSIZE_T bytesWritten);
};

#else

class Cr3Walker {
public:
    static ULONG64 TranslateVirtualToPhysical(ULONG64, ULONG64) { return 0; }
    static NTSTATUS ReadProcessMemoryCr3(ULONG64, ULONG64, PVOID, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
    static NTSTATUS WriteProcessMemoryCr3(ULONG64, ULONG64, const void*, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
};

#endif // UNPD_FEATURE_CR3_PML4_OPERATIONS

} // namespace unpd::mmu

#endif // UNPD_MMU_CR3_WALKER_HPP
