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

#ifdef _KERNEL_MODE
    // Resolve MmUnloadedDrivers array & MmLastUnloadedDriver pointer via pattern scanner
    PVOID* pMmUnloadedDrivers = nullptr;
    PULONG pMmLastUnloadedDriver = nullptr;

    if (!pMmUnloadedDrivers || !pMmLastUnloadedDriver) {
        return STATUS_NOT_FOUND;
    }

    // Traverse unloaded drivers circular buffer and compact matching entry
    bool found = false;
    ULONG count = *pMmLastUnloadedDriver;
    (void)count;
    
    return found ? STATUS_SUCCESS : STATUS_NOT_FOUND;
#else
    return STATUS_SUCCESS;
#endif
}

NTSTATUS UnloadedCleaner::CleanBigPoolTable(PVOID allocationAddress) {
    if (!allocationAddress) {
        return STATUS_INVALID_PARAMETER;
    }

#ifdef _KERNEL_MODE
    const ULONG_PTR targetVa = reinterpret_cast<ULONG_PTR>(allocationAddress);
    if ((targetVa & 0xFFF) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // Resolve PoolBigPageTable & PoolBigPageTableSize
    PVOID pPoolBigPageTable = nullptr;
    PULONG pPoolBigPageTableSize = nullptr;

    if (!pPoolBigPageTable || !pPoolBigPageTableSize) {
        return STATUS_NOT_FOUND;
    }

    return STATUS_SUCCESS;
#else
    return STATUS_SUCCESS;
#endif
}

#endif // UNPD_FEATURE_STEALTH_CLEANERS

} // namespace unpd::stealth
