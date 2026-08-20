#pragma once

#ifndef UNPD_CONFIG_HPP
#define UNPD_CONFIG_HPP

/**
 * @brief Master Template Configuration Header for UNPD-based Drivers.
 *
 * @details
 * Features can be individually toggled via preprocessor macros or C++20 constexpr flags.
 * Disabling a feature removes its code and binary footprint completely.
 */

// Project Metadata
#define UNPD_PROJECT_NAME           "Unsolicited Non-Paged Driver"
#define UNPD_PROJECT_VERSION_STRING "1.4.0"
#define UNPD_PROJECT_VERSION_MAJOR  1
#define UNPD_PROJECT_VERSION_MINOR  4
#define UNPD_PROJECT_VERSION_PATCH  0

// Kernel Device Names & Symbolic Links
#define UNPD_NT_DEVICE_NAME         L"\\Device\\UnsolicitedNonPagedDriver"
#define UNPD_DOS_DEVICE_NAME        L"\\DosDevices\\UnsolicitedNonPagedDriver"
#define UNPD_WIN32_DEVICE_PATH      L"\\\\.\\UnsolicitedNonPagedDriver"

// Memory Subsystem Pool Tag ('DPNU')
#define UNPD_MEMORY_POOL_TAG        'DPNU'

// Service Control Manager Name
#define UNPD_SERVICE_NAME           L"UnsolicitedNonPagedDriver"
#define UNPD_SERVICE_DISPLAY_NAME   L"Unsolicited Non-Paged Driver (UNPD)"

// Communication Backends
#define UNPD_COMM_BACKEND_IOCTL       0
#define UNPD_COMM_BACKEND_SHARED_MEM  1
#define UNPD_COMM_BACKEND_HOOK        2
#define UNPD_COMM_BACKEND_TCP_KSOCKET 3

// Modular Feature Flags (Toggle to 1 to enable, 0 to strip completely)
#ifndef UNPD_FEATURE_COMM_BACKEND
#define UNPD_FEATURE_COMM_BACKEND           UNPD_COMM_BACKEND_IOCTL
#endif

#ifndef UNPD_FEATURE_SYNTHETIC_MOUSE_INPUT
#define UNPD_FEATURE_SYNTHETIC_MOUSE_INPUT  1
#endif

#ifndef UNPD_FEATURE_PROCESS_BASE_QUERY
#define UNPD_FEATURE_PROCESS_BASE_QUERY     1
#endif

#ifndef UNPD_FEATURE_STEALTH_CLEANERS
#define UNPD_FEATURE_STEALTH_CLEANERS       1
#endif

#ifndef UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS
#define UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS 1
#endif

#ifndef UNPD_FEATURE_CR3_PML4_OPERATIONS
#define UNPD_FEATURE_CR3_PML4_OPERATIONS    1
#endif

#ifndef UNPD_FEATURE_PTE_REMAPPER
#define UNPD_FEATURE_PTE_REMAPPER           1
#endif

#ifndef UNPD_FEATURE_KERNEL_APC_INJECTION
#define UNPD_FEATURE_KERNEL_APC_INJECTION   1
#endif

#ifndef UNPD_FEATURE_HARDWARE_BREAKPOINTS
#define UNPD_FEATURE_HARDWARE_BREAKPOINTS   1
#endif

#ifndef UNPD_FEATURE_EMULATOR_SANDBOX
#define UNPD_FEATURE_EMULATOR_SANDBOX       1
#endif

// Security Configuration Toggles
#ifndef UNPD_CONFIG_STRICT_SDDL_ACL
#define UNPD_CONFIG_STRICT_SDDL_ACL          1
#endif

#ifndef UNPD_CONFIG_ALLOW_SIMD_ACCELERATION
#define UNPD_CONFIG_ALLOW_SIMD_ACCELERATION  1
#endif

#ifndef UNPD_CONFIG_PREFER_AVX512
#define UNPD_CONFIG_PREFER_AVX512            0
#endif

namespace unpd::config {

constexpr bool kFeatureStealthCleaners       = (UNPD_FEATURE_STEALTH_CLEANERS != 0);
constexpr bool kFeaturePhysicalMemoryAccess = (UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS != 0);
constexpr bool kFeatureCr3Pml4Operations    = (UNPD_FEATURE_CR3_PML4_OPERATIONS != 0);
constexpr bool kFeaturePteRemapper           = (UNPD_FEATURE_PTE_REMAPPER != 0);
constexpr bool kFeatureKernelApcInjection   = (UNPD_FEATURE_KERNEL_APC_INJECTION != 0);
constexpr bool kFeatureHardwareBreakpoints   = (UNPD_FEATURE_HARDWARE_BREAKPOINTS != 0);

} // namespace unpd::config

#endif // UNPD_CONFIG_HPP
