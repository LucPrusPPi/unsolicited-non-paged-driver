#pragma once

#ifndef UNPD_CONFIG_HPP
#define UNPD_CONFIG_HPP

/**
 * @brief Master Template Configuration Header for UNPD-based Drivers.
 *
 * @details
 * Modify these constants to customize driver naming, device paths, pool tags,
 * and service parameters for your specific driver implementation.
 */

// Project Metadata
#define UNPD_PROJECT_NAME           "Unsolicited Non-Paged Driver"
#define UNPD_PROJECT_VERSION_STRING "1.0.0"
#define UNPD_PROJECT_VERSION_MAJOR  1
#define UNPD_PROJECT_VERSION_MINOR  0
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

#endif // UNPD_CONFIG_HPP
