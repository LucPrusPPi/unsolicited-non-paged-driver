#pragma once

#ifndef UNPD_STEALTH_UNLOADED_DRIVERS_HPP
#define UNPD_STEALTH_UNLOADED_DRIVERS_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::stealth {

#if UNPD_FEATURE_STEALTH_CLEANERS

struct UnloadedDriverEntry {
    UNICODE_STRING Name;
    PVOID StartAddress;
    PVOID EndAddress;
    LARGE_INTEGER CurrentTime;
};

class UnloadedDrivers {
public:
    static NTSTATUS CleanUnloadedDrivers(PCUNICODE_STRING driverName);
    static NTSTATUS CleanBigPoolTable(PVOID allocationAddress);
};

using UnloadedCleaner = UnloadedDrivers;

#else

class UnloadedDrivers {
public:
    static NTSTATUS CleanUnloadedDrivers(PCUNICODE_STRING) { return STATUS_NOT_SUPPORTED; }
    static NTSTATUS CleanBigPoolTable(PVOID) { return STATUS_NOT_SUPPORTED; }
};

using UnloadedCleaner = UnloadedDrivers;

#endif // UNPD_FEATURE_STEALTH_CLEANERS

} // namespace unpd::stealth

#endif // UNPD_STEALTH_UNLOADED_DRIVERS_HPP
