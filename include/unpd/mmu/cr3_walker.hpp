#pragma once

#ifndef UNPD_MMU_CR3_WALKER_HPP
#define UNPD_MMU_CR3_WALKER_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>
#include <unpd/mmu/paging_types.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_CR3_PML4_OPERATIONS

class Cr3Walker {
public:
    static ULONG64 TranslateVirtualToPhysical(ULONG64 cr3, ULONG64 virtualAddress);
    static NTSTATUS ReadProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead);
    static NTSTATUS WriteProcessMemoryCr3(ULONG64 cr3, ULONG64 virtualAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten);
};

#else

class Cr3Walker {
public:
    static ULONG64 TranslateVirtualToPhysical(ULONG64, ULONG64) { return 0; }
    static NTSTATUS ReadProcessMemoryCr3(ULONG64, ULONG64, PVOID, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
    static NTSTATUS WriteProcessMemoryCr3(ULONG64, ULONG64, PVOID, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
};

#endif // UNPD_FEATURE_CR3_PML4_OPERATIONS

} // namespace unpd::mmu

#endif // UNPD_MMU_CR3_WALKER_HPP
