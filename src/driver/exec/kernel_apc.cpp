#include <unpd/common.h>
#include <unpd/exec/kernel_apc.hpp>

namespace unpd::exec {

#if UNPD_FEATURE_KERNEL_APC_INJECTION

NTSTATUS KernelApc::QueueUserApc(HANDLE targetThreadId, PVOID userRoutine, PVOID userContext) {
    if (!targetThreadId || !userRoutine) return STATUS_INVALID_PARAMETER;
    (void)userContext;
    return STATUS_SUCCESS;
}

#endif // UNPD_FEATURE_KERNEL_APC_INJECTION

} // namespace unpd::exec
