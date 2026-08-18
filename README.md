<div align="center">

# Unsolicited Non-Paged Driver (UNPD)

An academic-grade, modern C++20 Windows Kernel Driver template, test harness, and automated test-signing pipeline.

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg?style=flat-square)]()
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square)]()
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20x64-informational.svg?style=flat-square)]()
[![Architecture](https://img.shields.io/badge/architecture-WDM%20%2F%20NonPagedPoolNx-orange.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)]()

</div>

---

## Motivation and Design Goals

Writing Windows kernel drivers often involves navigating through legacy MSDN samples that rely heavily on C-style goto error handling, outdated memory allocation APIs, and fragile manual cleanup sequences. UNPD is designed as an idiomatic reference implementation for modern Windows kernel development on x64 architectures.

The project demonstrates how modern C++ features can be safely used inside the NT kernel when compiled with native kernel flags (`/kernel /GR- /EHsc-`). It eliminates manual cleanup bugs through RAII synchronization wrappers, tracks non-paged pool memory allocations internally, and provides full Structured Exception Handling protection against malicious or corrupted user-mode buffers.

Included with the driver is a complete test harness and PowerShell automation suite for generating test certificates, signing binaries, and managing kernel services in virtualized test environments.

---

## Core Engineering Features

### Modern C++ in Kernel Space
Compiled with Microsoft Visual C++ using the `/kernel` switch with runtime type information and C++ exceptions disabled. Synchronization primitives such as `KSPIN_LOCK` and `FAST_MUTEX` are wrapped in zero-cost RAII guards (`unpd::SpinlockGuard` and `unpd::FastMutexGuard`). These wrappers guarantee that locks are released and IRQL levels are restored upon leaving scope, even during early exit conditions.

### Defensive Memory Management
All dynamic memory allocations target non-executable non-paged pool memory via `ExAllocatePool2` with `POOL_FLAG_NON_PAGED` and custom pool tagging (`'UNPD'`). This prevents kernel data execution vulnerabilities and automatically zeroes allocated memory blocks to avoid information disclosure. The driver maintains an internal doubly-linked list (`LIST_ENTRY`) of active allocations, ensuring that all lingering buffers are safely deallocated during `DriverUnload`.

### Robust I/O Dispatching
The driver implements reference handlers for three Windows I/O transfer methods:
- `METHOD_BUFFERED`: Used for structured control packets, with request and response validation.
- `METHOD_OUT_DIRECT`: Demonstrates zero-copy direct memory access using Memory Descriptor Lists (`MDL`) mapped into system space via `MmGetSystemAddressForMdlSafe`.
- `METHOD_NEITHER`: Handles raw user-mode virtual addresses by performing explicit alignment verification and probing through `ProbeForRead` and `ProbeForWrite` enclosed in `__try` / `__except` blocks.

### Automated Test Pipeline
A multi-threaded usermode client (`unpd_tests.exe`) validates every IOCTL opcode, tests buffer boundary edge cases, performs adversarial fuzzing, and stress-tests synchronization mechanisms across 16 concurrent threads.

---

## Architecture

```
 +-------------------------------------------------------------------+
 |                   Usermode Client / Test Suite                    |
 |   [ unpd_tests.exe ] <---------> [ Win32 DeviceIoControl ]        |
 +-------------------------------------------------------------------+
                                   |
                     \\.\UnsolicitedNonPagedDriver
                                   |
 +-------------------------------------------------------------------+
 |                    Windows Kernel (I/O Manager)                   |
 |              IRP Dispatcher & Security Access Validation          |
 +-------------------------------------------------------------------+
                                   |
                                   v
 +-------------------------------------------------------------------+
 |                     UNPD Core Engine (unpd.sys)                   |
 |                                                                   |
 |   +-----------------------+     +-----------------------------+   |
 |   |   IRP Dispatch Table  |     |    Buffer Safety Validator  |   |
 |   |  - IRP_MJ_CREATE      |     |  - Structured Exception Hnd |   |
 |   |  - IRP_MJ_CLOSE       |     |  - ProbeForRead / Write     |   |
 |   |  - IRP_MJ_DEV_CONTROL |     |  - Alignment Verification   |   |
 |   +-----------------------+     +-----------------------------+   |
 |               |                                |                  |
 |               +----------------+---------------+                  |
 |                                |                                  |
 |                                v                                  |
 |   +-----------------------------------------------------------+   |
 |   |            Non-Paged Pool & Handle Manager                |   |
 |   |  - ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'UNPD')     |   |
 |   |  - Doubly-Linked Allocation Table (Spinlock Protected)    |   |
 |   |  - Deterministic Resource Teardown on DriverUnload        |   |
 |   +-----------------------------------------------------------+   |
 +-------------------------------------------------------------------+
```

---

## Building and Signing

### Requirements
- Windows 10 or Windows 11 x64
- Visual Studio 2022 (MSVC v143 or higher)
- Windows SDK and WDK (10.0.26100.0 or 10.0.28000.0+)
- CMake 3.20+ and Ninja

### Build Execution
To build the driver and test suite, run the build script from PowerShell:

```powershell
.\build.ps1 -Config Release -Sign
```

The script configures the MSVC environment, compiles the kernel binary `unpd.sys`, builds the user-mode test harness `unpd_tests.exe`, generates a self-signed X.509 code signing certificate with Microsoft-compatible extensions, and signs the driver with SHA-256 and DigiCert timestamping.

Compiled artifacts are placed in `build/bin/`:
- `unpd.sys`: Kernel driver binary
- `unpd_tests.exe`: Usermode test runner
- `unpd_test_root.cer`: Exported public test certificate

---

## Testing in VMWare Workstation

Running unsigned or test-signed kernel drivers requires enabling test signing on the target system.

1. Disable Secure Boot in the VMWare virtual machine settings under Options -> Advanced.
2. Copy `build/bin/unpd.sys`, `build/bin/unpd_tests.exe`, `unpd_test_root.cer`, and the `scripts/` folder into the virtual machine.
3. Open an Administrator PowerShell inside the VM and run the setup script:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\Setup-VM.ps1 -CertPath .\unpd_test_root.cer
   shutdown /r /t 0
   ```
4. After rebooting into Test Mode, execute the end-to-end test runner:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\Run-Tests.ps1 -SkipBuild
   ```

---

## Test Suite Matrix

The user-mode test runner executes a structured validation suite covering functional correctness, memory tracking, buffer fuzzing, and concurrency stress testing.

| Test Case | Type | Description | Expected Status |
|---|---|---|---|
| Ioctl_Ping_ValidSequence | Functional | Round-trip ping with sequence validation and high-precision kernel timestamps | STATUS_SUCCESS |
| Ioctl_AllocateAndFree | Functional | Allocates non-paged pool memory, verifies unique handle generation, and frees | STATUS_SUCCESS |
| Ioctl_AllocateMultiple | Functional | Allocates 16 concurrent buffers and asserts handle table integrity | STATUS_SUCCESS |
| Ioctl_FreeInvalidHandle | Negative | Attempts to free a non-existent handle (0xDEADBEEFCAFEBABE) | STATUS_NOT_FOUND |
| Ioctl_QueryStats | Telemetry | Queries active allocations, total bytes processed, and spinlock contention | STATUS_SUCCESS |
| Fuzzing_Ping_TruncatedBuffer | Fuzzing | Sends an undersized input buffer to verify length validation | STATUS_BUFFER_TOO_SMALL |
| Fuzzing_Ping_InvalidMagic | Fuzzing | Transmits corrupt magic header bytes | STATUS_INVALID_PARAMETER |
| Fuzzing_Allocate_ZeroBytes | Boundary | Requests a zero-byte memory allocation | STATUS_INVALID_PARAMETER |
| Fuzzing_Allocate_Oversized | Boundary | Requests an allocation exceeding size limits (1 TB) | STATUS_INVALID_PARAMETER |
| Fuzzing_ProcessNeither_Null | Security | Passes null pointers to METHOD_NEITHER handler trapped by SEH | Caught safely |
| Stress_ConcurrentPing_16Th | Stress | 16 worker threads firing 8,000 IOCTL requests simultaneously | 100% Pass, 0 Drops |
| Stress_ConcurrentAllocFree | Stress | High-frequency concurrent allocation and deallocation across 8 threads | Zero Race Conditions |

---

## Repository Layout

```
.
├── .claude/notes/              # Session and development logs
├── .gemini/rules/              # Engineering policy rules
├── .gemini/skills/             # Driver dev skills
├── cmake/                      # CMake toolchain configuration
├── docs/                       # Technical documentation
│   ├── ARCHITECTURE.md         # Kernel internals and memory layout
│   ├── IOCTL_PROTOCOL.md       # IOCTL packet specifications
│   └── VM_SETUP.md             # VMWare testmode and WinDbg guide
├── include/unpd/               # Header files
│   ├── common.h                # IOCTL opcodes, constants, and packet structs
│   ├── dispatch.hpp            # IRP dispatch declarations and device extension
│   ├── kernel_raii.hpp         # RAII primitives for kernel locks and memory
│   └── security.hpp            # User buffer validation and probing helpers
├── src/
│   ├── driver/                 # Driver implementation (unpd.sys)
│   │   ├── driver_entry.cpp    # DriverEntry and deterministic DriverUnload teardown
│   │   ├── device_control.cpp  # IRP_MJ_CREATE, IRP_MJ_CLOSE, IRP_MJ_DEVICE_CONTROL
│   │   ├── ioctl_handler.cpp   # Safe IOCTL handler logic
│   │   ├── memory_manager.cpp  # Pool allocation tracking and handle table
│   │   └── unpd.rc             # Windows driver version info and metadata
│   └── tests/                  # Usermode test suite (unpd_tests.exe)
│       ├── test_client.hpp     # Win32 driver client wrapper
│       ├── test_runner.cpp     # Test runner entrypoint and execution timer
│       ├── test_ioctl.cpp      # Functional IOCTL verification
│       ├── test_fuzzing.cpp    # Adversarial boundary and buffer fuzzing tests
│       └── test_stress.cpp     # Multithreaded concurrency stress tests
├── scripts/                    # PowerShell automation scripts
│   ├── Deploy-Driver.ps1       # SCM driver service manager (create, start, stop, delete)
│   ├── Run-Tests.ps1           # End-to-end test execution pipeline
│   ├── Setup-VM.ps1            # VM testmode and root certificate installer
│   └── Sign-Driver.ps1         # Certificate generation and SignTool automation
├── build.ps1                   # One-click build script
├── CMakeLists.txt              # CMake configuration
├── CMakePresets.json           # Ready-to-use presets
├── vcpkg.json                  # vcpkg manifest
└── LICENSE                     # MIT License
```

---

## Detailed Documentation

- [Kernel Architecture & Memory Lifecycle](docs/ARCHITECTURE.md)
- [IOCTL Protocol & Packet Formats](docs/IOCTL_PROTOCOL.md)
- [VMWare & WinDbg KDNET Setup Guide](docs/VM_SETUP.md)

---

## License

This project is licensed under the [MIT License](LICENSE).
