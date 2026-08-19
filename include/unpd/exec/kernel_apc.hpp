#pragma once

#ifndef UNPD_EXEC_KERNEL_APC_HPP
#define UNPD_EXEC_KERNEL_APC_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

namespace unpd::exec {

#if UNPD_FEATURE_KERNEL_APC_INJECTION

typedef void (*KernelApcRoutine)(PVOID normalContext, PVOID sysArg1, PVOID sysArg2);

class KernelApc {
public:
    static NTSTATUS QueueUserApc(HANDLE targetThreadId, PVOID userRoutine, PVOID userContext);
};

#else

class KernelApc {
public:
    static NTSTATUS QueueUserApc(HANDLE, PVOID, PVOID) { return STATUS_NOT_SUPPORTED; }
};

#endif // UNPD_FEATURE_KERNEL_APC_INJECTION

} // namespace unpd::exec

#endif // UNPD_EXEC_KERNEL_APC_HPP
