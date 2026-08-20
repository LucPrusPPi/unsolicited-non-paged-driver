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

#include <unpd/kernel_asm.hpp>

namespace unpd::stealth {

#if UNPD_FEATURE_STEALTH_CLEANERS

[[maybe_unused]] static const uint8_t* FindPatternInternal(const uint8_t* base, SIZE_T size, const uint8_t* pattern, const char* mask) {
    return static_cast<const uint8_t*>(UnpdScanPatternASM(base, size, pattern, mask));
}

NTSTATUS PiDdbCleaner::CleanDriverTrace(PCUNICODE_STRING driverName, ULONG timeDateStamp) {
    if (!driverName || !driverName->Buffer || driverName->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

#ifdef _KERNEL_MODE
    PiDdbCacheEntry searchEntry{};
    searchEntry.DriverName = *driverName;
    searchEntry.TimeDateStamp = timeDateStamp;

    // TODO: Implement signature scanning for PiDDBCacheTable and PiDDBLock in ntoskrnl.exe PAGE section
    PRTL_AVL_TABLE pTable = nullptr;
    if (!pTable) {
        return STATUS_NOT_FOUND;
    }

    PVOID entry = RtlLookupElementGenericTableAvl(pTable, &searchEntry);
    if (!entry) {
        return STATUS_NOT_FOUND;
    }

    auto* cacheEntry = static_cast<PiDdbCacheEntry*>(entry);
    UnpdListRemoveEntryASM(&cacheEntry->List);

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
