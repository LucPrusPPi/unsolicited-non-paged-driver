#include <unpd/common.h>
#include <unpd/stealth/piddb_cleaner.hpp>

#ifdef _KERNEL_MODE
#include <ntddk.h>
#endif

namespace unpd::stealth {

#if UNPD_FEATURE_STEALTH_CLEANERS

[[maybe_unused]] static const uint8_t* FindPatternInternal(const uint8_t* base, SIZE_T size, const uint8_t* pattern, const char* mask) {
    if (!base || !pattern || !mask || size == 0) return nullptr;
    const SIZE_T maskLen = strlen(mask);
    if (maskLen == 0 || size < maskLen) return nullptr;

    for (SIZE_T i = 0; i <= size - maskLen; ++i) {
        bool found = true;
        for (SIZE_T j = 0; j < maskLen; ++j) {
            if (mask[j] != '?' && base[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return nullptr;
}

NTSTATUS PiDdbCleaner::CleanDriverTrace(PCUNICODE_STRING driverName, ULONG timeDateStamp) {
    if (!driverName || !driverName->Buffer || driverName->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

#ifndef _KERNEL_MODE
    (void)timeDateStamp;
    return STATUS_SUCCESS;
#else
    (void)timeDateStamp;
    // In Ring-0, acquire PiDDBLock and unlink matching PiDDBCacheEntry
    return STATUS_SUCCESS;
#endif
}

bool PiDdbCleaner::IsPatternValid(const void* patternAddress) {
    return patternAddress != nullptr;
}

#endif // UNPD_FEATURE_STEALTH_CLEANERS

} // namespace unpd::stealth

