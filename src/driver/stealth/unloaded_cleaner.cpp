#include <unpd/common.h>
#include <unpd/stealth/unloaded_cleaner.hpp>

namespace unpd::stealth {

#if UNPD_FEATURE_STEALTH_CLEANERS

NTSTATUS UnloadedCleaner::CleanUnloadedDrivers(PCUNICODE_STRING driverName) {
    if (!driverName || !driverName->Buffer) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS UnloadedCleaner::CleanBigPoolTable(PVOID allocationAddress) {
    if (!allocationAddress) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

#endif // UNPD_FEATURE_STEALTH_CLEANERS

} // namespace unpd::stealth
