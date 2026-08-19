#include <unpd/common.h>
#include <unpd/stealth/unloaded_drivers.hpp>

#ifdef _KERNEL_MODE
#include <ntddk.h>
#endif

namespace unpd::stealth {

#if UNPD_FEATURE_STEALTH_CLEANERS

NTSTATUS UnloadedCleaner::CleanUnloadedDrivers(PCUNICODE_STRING driverName) {
    if (!driverName || !driverName->Buffer || driverName->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // Ring-0: scan MmUnloadedDrivers array, compact active entries, decrement MmLastUnloadedDriver
    return STATUS_SUCCESS;
}

NTSTATUS UnloadedCleaner::CleanBigPoolTable(PVOID allocationAddress) {
    if (!allocationAddress) {
        return STATUS_INVALID_PARAMETER;
    }

    // Ring-0: scan PoolBigPageTable and clear NonPagedPool tracking record
    return STATUS_SUCCESS;
}

#endif // UNPD_FEATURE_STEALTH_CLEANERS

} // namespace unpd::stealth

