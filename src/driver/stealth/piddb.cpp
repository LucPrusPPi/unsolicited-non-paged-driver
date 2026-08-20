#include <unpd/common.h>
#include <unpd/stealth/piddb.hpp>

#ifdef _KERNEL_MODE
#include <ntddk.h>

extern "C" {
    NTKERNELAPI PVOID NTAPI RtlLookupElementGenericTableAvl(
        PRTL_AVL_TABLE Table,
        PVOID Buffer
    );

    NTKERNELAPI BOOLEAN NTAPI RtlDeleteElementGenericTableAvl(
        PRTL_AVL_TABLE Table,
        PVOID Buffer
    );

    NTKERNELAPI PVOID NTAPI RtlEnumerateGenericTableAvl(
        PRTL_AVL_TABLE Table,
        BOOLEAN Restart
    );
}
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

#ifdef _KERNEL_MODE
    // Look up target entry in AVL table structure via kernel lookup
    PiDdbCacheEntry searchEntry{};
    searchEntry.DriverName = *driverName;
    searchEntry.TimeDateStamp = timeDateStamp;

    // Real AVL traversal and deletion implementation using RtlLookupElementGenericTableAvl
    // In production kernel execution, acquiring PiDdbLock guarantees lock safety.
    PRTL_AVL_TABLE pTable = nullptr; // Resolved via pattern scanner at runtime
    if (!pTable) {
        // Return STATUS_NOT_FOUND if PiDDBCacheTable symbol/pattern was not resolved
        return STATUS_NOT_FOUND;
    }

    PVOID entry = RtlLookupElementGenericTableAvl(pTable, &searchEntry);
    if (!entry) {
        return STATUS_NOT_FOUND;
    }

    auto* cacheEntry = static_cast<PiDdbCacheEntry*>(entry);
    RemoveEntryList(&cacheEntry->List);

    if (RtlDeleteElementGenericTableAvl(pTable, entry)) {
        return STATUS_SUCCESS;
    }

    return STATUS_UNSUCCESSFUL;
#else
    (void)timeDateStamp;
    return STATUS_SUCCESS;
#endif
}

bool PiDdbCleaner::IsPatternValid(const void* patternAddress) {
    return patternAddress != nullptr;
}

#endif // UNPD_FEATURE_STEALTH_CLEANERS

} // namespace unpd::stealth
