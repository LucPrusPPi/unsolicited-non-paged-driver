#pragma once

#ifndef UNPD_EXEC_APC_HPP
#define UNPD_EXEC_APC_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::exec {

#if UNPD_FEATURE_KERNEL_APC_INJECTION

typedef void (*KernelApcRoutine)(PVOID normalContext, PVOID sysArg1, PVOID sysArg2);

class ApcDispatcher {
public:
    static NTSTATUS QueueUserApc(HANDLE targetThreadId, PVOID userRoutine, PVOID userContext);
};

using KernelApc = ApcDispatcher;

#else

class ApcDispatcher {
public:
    static NTSTATUS QueueUserApc(HANDLE, PVOID, PVOID) { return STATUS_NOT_SUPPORTED; }
};

using KernelApc = ApcDispatcher;

#endif // UNPD_FEATURE_KERNEL_APC_INJECTION

} // namespace unpd::exec

#endif // UNPD_EXEC_APC_HPP
