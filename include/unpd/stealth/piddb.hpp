#pragma once

#ifndef UNPD_STEALTH_PIDDB_HPP
#define UNPD_STEALTH_PIDDB_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::stealth {

#if UNPD_FEATURE_STEALTH_CLEANERS

struct PiDdbCacheEntry {
    LIST_ENTRY List;
    UNICODE_STRING DriverName;
    ULONG TimeDateStamp;
    NTSTATUS LoadStatus;
};

class PiDdbCleaner {
public:
    static NTSTATUS CleanDriverTrace(PCUNICODE_STRING driverName, ULONG timeDateStamp);
    static bool IsPatternValid(const void* patternAddress);
};

#else

class PiDdbCleaner {
public:
    static NTSTATUS CleanDriverTrace(PCUNICODE_STRING, ULONG) { return STATUS_NOT_SUPPORTED; }
    static bool IsPatternValid(const void*) { return false; }
};

#endif // UNPD_FEATURE_STEALTH_CLEANERS

} // namespace unpd::stealth

#endif // UNPD_STEALTH_PIDDB_HPP
