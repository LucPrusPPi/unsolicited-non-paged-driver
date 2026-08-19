#include <unpd/common.h>
#include <unpd/exec/kernel_apc.hpp>

#ifdef _KERNEL_MODE
#include <ntddk.h>

extern "C" {
    NTKERNELAPI NTSTATUS PsLookupThreadByThreadId(HANDLE ThreadId, PETHREAD *Thread);

    typedef enum _KAPC_ENVIRONMENT {
        OriginalApcEnvironment,
        AttachedApcEnvironment,
        CurrentApcEnvironment,
        InsertApcEnvironment
    } KAPC_ENVIRONMENT;

    typedef VOID (*PKNORMAL_ROUTINE)(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument2);
    typedef VOID (*PKKERNEL_ROUTINE)(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2);
    typedef VOID (*PKRUNDOWN_ROUTINE)(PKAPC Apc);

    NTKERNELAPI VOID KeInitializeApc(
        PRKAPC Apc,
        PRKTHREAD Thread,
        KAPC_ENVIRONMENT Environment,
        PKKERNEL_ROUTINE KernelRoutine,
        PKRUNDOWN_ROUTINE RundownRoutine,
        PKNORMAL_ROUTINE NormalRoutine,
        KPROCESSOR_MODE ProcessorMode,
        PVOID NormalContext
    );

    NTKERNELAPI BOOLEAN KeInsertQueueApc(
        PRKAPC Apc,
        PVOID SystemArgument1,
        PVOID SystemArgument2,
        KPRIORITY Increment
    );
}

static VOID KernelApcCleanup(PKAPC apc, PKNORMAL_ROUTINE*, PVOID*, PVOID*, PVOID*) {
    ExFreePoolWithTag(apc, UNPD_POOL_TAG);
}
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
        KernelApcCleanup,
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
