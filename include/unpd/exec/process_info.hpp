#pragma once

#ifndef UNPD_EXEC_PROCESS_INFO_HPP
#define UNPD_EXEC_PROCESS_INFO_HPP

#include "unpd/config.hpp"
#include "unpd/common.h"

#ifdef _KERNEL_MODE
#include <ntddk.h>

extern "C" NTKERNELAPI NTSTATUS NTAPI PsLookupProcessByProcessId(_In_ HANDLE ProcessId, _Outptr_ PEPROCESS *Process);
extern "C" NTKERNELAPI PVOID NTAPI PsGetProcessSectionBaseAddress(_In_ PEPROCESS Process);
extern "C" NTKERNELAPI PVOID NTAPI PsGetProcessPeb(_In_ PEPROCESS Process);
#endif

namespace unpd::exec {

struct ProcessInfo {
    uint64_t SectionBaseAddress;
    uint64_t PebAddress;
};

class ProcessInfoEngine {
public:
    static NTSTATUS QueryProcessInfo(uint32_t processId, ProcessInfo& outInfo) noexcept {
        outInfo = {};
#if UNPD_FEATURE_PROCESS_BASE_QUERY && defined(_KERNEL_MODE)
        PEPROCESS process = nullptr;
        NTSTATUS status = PsLookupProcessByProcessId(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(processId)), &process);
        if (!NT_SUCCESS(status) || !process) {
            return STATUS_NOT_FOUND;
        }

        outInfo.SectionBaseAddress = reinterpret_cast<uint64_t>(PsGetProcessSectionBaseAddress(process));
        outInfo.PebAddress = reinterpret_cast<uint64_t>(PsGetProcessPeb(process));

        ObDereferenceObject(process);
        return STATUS_SUCCESS;
#else
        UNREFERENCED_PARAMETER(processId);
        return STATUS_NOT_SUPPORTED;
#endif
    }
};

} // namespace unpd::exec

#endif // UNPD_EXEC_PROCESS_INFO_HPP
