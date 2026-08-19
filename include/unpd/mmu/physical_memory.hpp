#pragma once

#ifndef UNPD_MMU_PHYSICAL_MEMORY_HPP
#define UNPD_MMU_PHYSICAL_MEMORY_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::mmu {

#if UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

class PhysicalMemory {
public:
    static NTSTATUS ReadPhysicalAddress(ULONG64 physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead);
    static NTSTATUS WritePhysicalAddress(ULONG64 physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesWritten);
};

#else

class PhysicalMemory {
public:
    static NTSTATUS ReadPhysicalAddress(ULONG64, PVOID, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
    static NTSTATUS WritePhysicalAddress(ULONG64, PVOID, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
};

#endif // UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

} // namespace unpd::mmu

#endif // UNPD_MMU_PHYSICAL_MEMORY_HPP
