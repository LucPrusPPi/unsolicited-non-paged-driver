#pragma once

#ifndef UNPD_SECURITY_HPP
#define UNPD_SECURITY_HPP

#ifdef _KERNEL_MODE

#include <ntddk.h>

namespace unpd {

inline NTSTATUS ProbeUserBufferForRead(
    const void* buffer,
    SIZE_T length,
    ULONG alignment
) noexcept {
    if (buffer == nullptr || length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (ExGetPreviousMode() != UserMode) {
        return STATUS_SUCCESS;
    }

    __try {
        ProbeForRead(const_cast<void*>(buffer), length, alignment);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    return STATUS_SUCCESS;
}

inline NTSTATUS ProbeUserBufferForWrite(
    void* buffer,
    SIZE_T length,
    ULONG alignment
) noexcept {
    if (buffer == nullptr || length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (ExGetPreviousMode() != UserMode) {
        return STATUS_SUCCESS;
    }

    __try {
        ProbeForWrite(buffer, length, alignment);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    return STATUS_SUCCESS;
}

} // namespace unpd

#endif // _KERNEL_MODE
#endif // UNPD_SECURITY_HPP
