#include <unpd/common.h>
#include <unpd/exec/kernel_apc.hpp>

#ifdef _KERNEL_MODE
#include <ntddk.h>
#endif

namespace unpd::exec {

#if UNPD_FEATURE_KERNEL_APC_INJECTION

NTSTATUS KernelApc::QueueUserApc(HANDLE targetThreadId, PVOID userRoutine, PVOID userContext) {
    if (!targetThreadId || !userRoutine) {
        return STATUS_INVALID_PARAMETER;
    }

#ifndef _KERNEL_MODE
    (void)userContext;
    return STATUS_SUCCESS;
#else
    PETHREAD thread = nullptr;
    NTSTATUS status = PsLookupThreadByThreadId(targetThreadId, &thread);
    if (status != STATUS_SUCCESS || !thread) {
        return status;
    }

    auto* apc = static_cast<PKAPC>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KAPC), UNPD_POOL_TAG));
    if (!apc) {
        ObDereferenceObject(thread);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeInitializeApc(
        apc,
        reinterpret_cast<PKTHREAD>(thread),
        OriginalApcEnvironment,
        [](PKAPC apcPtr, PKNORMAL_ROUTINE*, PVOID*, PVOID*, PVOID*) {
            ExFreePoolWithTag(apcPtr, UNPD_POOL_TAG);
        },
        nullptr,
        reinterpret_cast<PKNORMAL_ROUTINE>(userRoutine),
        UserMode,
        userContext
    );

    if (!KeInsertQueueApc(apc, userContext, nullptr, 0)) {
        ExFreePoolWithTag(apc, UNPD_POOL_TAG);
        ObDereferenceObject(thread);
        return STATUS_UNSUCCESSFUL;
    }

    ObDereferenceObject(thread);
    return STATUS_SUCCESS;
#endif
}

#endif // UNPD_FEATURE_KERNEL_APC_INJECTION

} // namespace unpd::exec
