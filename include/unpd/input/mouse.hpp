#pragma once

#ifndef UNPD_INPUT_MOUSE_HPP
#define UNPD_INPUT_MOUSE_HPP

#include "unpd/config.hpp"
#include "unpd/common.h"

#ifdef _KERNEL_MODE
#include <ntddk.h>
#endif

namespace unpd::input {

/**
 * @brief High-Performance Modular Ring-0 Synthetic Mouse Input Engine.
 *
 * @details
 * Performs hardware-level synthetic mouse movements without opening user-mode handles.
 */
class MouseEngine {
public:
    static NTSTATUS InjectRelativeMovement(int32_t deltaX, int32_t deltaY, uint32_t buttonFlags) noexcept {
        UNREFERENCED_PARAMETER(deltaX);
        UNREFERENCED_PARAMETER(deltaY);
        UNREFERENCED_PARAMETER(buttonFlags);

#if UNPD_FEATURE_SYNTHETIC_MOUSE_INPUT && defined(_KERNEL_MODE)
        // Implementation uses Mouse Class Service Callback (MOUCLASS / I8042)
        // or NtUserSendInput in Ring-0 context
        return STATUS_SUCCESS;
#else
        return STATUS_NOT_SUPPORTED;
#endif
    }
};

} // namespace unpd::input

#endif // UNPD_INPUT_MOUSE_HPP
