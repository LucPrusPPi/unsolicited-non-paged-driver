#pragma once

#ifndef UNPD_MMU_PTE_REMAPPER_HPP
#define UNPD_MMU_PTE_REMAPPER_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_PTE_REMAPPER

class PteRemapper {
public:
    static NTSTATUS MakePageWritable(ULONG64 cr3, ULONG64 virtualAddress);
    static NTSTATUS MakePageExecutable(ULONG64 cr3, ULONG64 virtualAddress);
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
