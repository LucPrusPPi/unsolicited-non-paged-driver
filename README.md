<div align="center">

# Unsolicited Non-Paged Driver (UNPD)

An academic-grade, modern C++20 Windows Kernel Driver framework, zero-copy shared memory engine, and automated GoogleTest test-signing harness.

[![CI Pipeline](https://img.shields.io/badge/build-passing-brightgreen.svg?style=flat-square)]()
[![GoogleTest](https://img.shields.io/badge/tests-22%20passed-success.svg?style=flat-square)]()
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square)]()
[![Architecture](https://img.shields.io/badge/arch-x64%20%2F%20MASM64-orange.svg?style=flat-square)]()
[![Memory Subsystem](https://img.shields.io/badge/pool-NonPagedPoolNx%20%2F%20MDL-yellow.svg?style=flat-square)]()
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20x64-informational.svg?style=flat-square)]()
[![Package Manager](https://img.shields.io/badge/vcpkg-supported-purple.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)]()

</div>

---

## Overview and Motivation

Windows kernel driver development has historically suffered from fragmented C-style boilerplates, error-prone manual memory deallocation sequences, and fragile testing workflows. UNPD establishes a modern, idiomatically structured baseline for 64-bit Windows NT driver engineering.

Built from the ground up for Windows 10 and Windows 11 x64, UNPD demonstrates:
- How to write native C++20 kernel drivers with zero-cost RAII synchronization guards.
- Zero-copy shared memory ring-buffering between user space and kernel space via physical MDL mapping.
- Lock-free atomic double-buffering page swaps for high-throughput sensor/event streaming.
- Complete GoogleTest (GTest) automated test harness running natively on developer machines, GitHub Actions CI, and virtualized Windows Test Mode targets.

---

## Architecture Overview

```
 +-------------------------------------------------------------------+
 |                   Usermode Client / Test Suite                    |
 |                 [ unpd_tests.exe (GoogleTest) ]                   |
 +-------------------------------------------------------------------+
       | (Direct User Virtual Address)               | (DeviceIoControl)
       |                                             |
       v                                             v
 +-----------------------------+     +-------------------------------+
 |  Mapped Shared Memory Pages |     |  \\.\UnsolicitedNonPagedDriver|
 |  [ Active Page Buffer A ]   |     +-------------------------------+
 |  [ Standby Page Buffer B ]  |                     |
 +-----------------------------+                     v
       ^                             +-------------------------------+
       | (Atomic Swap & Mapping)     |     I/O Manager & Security    |
       |                             |     - Structured Exception    |
 +-----------------------------+     |     - ProbeForRead / Write    |
 |    UNPD Core (unpd.sys)     |     +-------------------------------+
 |  - MmAllocatePagesForMdlEx  |                     |
 |  - MmMapLockedPages         |                     v
 |  - Slab Cache (64B..4KB)    |     +-------------------------------+
 |  - RAII Spinlocks / Mutexes |     |    Non-Paged Pool Manager     |
 |  - MASM64 Memory Barriers   |     |    ExAllocatePool2 ('UNPD')   |
 +-----------------------------+     +-------------------------------+
```

---

## Key Technical Subsystems

### 1. Zero-Copy Shared Memory & Double Buffering
Instead of paying the cost of IRP dispatching and kernel-to-user buffer copying on every transaction, UNPD allocates contiguous physical pages using `MmAllocatePagesForMdlEx` and maps them directly into the calling user process address space using `MmMapLockedPagesSpecifyCache` with `UserMode` and `MdlMappingNoExecute`.

- **Atomic Buffer Swapping**: The driver controller swaps the active and standby buffers using `InterlockedExchange` and MASM64 hardware memory fences (`mfence`), providing microsecond-level synchronization without system call overhead.
- **Slab Cache Allocator**: Features fixed-size slab pools for 64-byte, 256-byte, 1024-byte, and 4096-byte blocks with lockless free-list caching to eliminate non-paged pool fragmentation.

### 2. Modern C++20 RAII in Kernel Space
Compiled with MSVC and Clang-CL using `/kernel /GR- /EHsc-`:
- `unpd::SpinlockGuard`: Acquires `KSPIN_LOCK` and restores the previous IRQL level upon destruction.
- `unpd::FastMutexGuard`: Manages `FAST_MUTEX` acquisition at `APC_LEVEL`.
- `unpd::PoolAllocation`: RAII smart pointer wrapping `ExAllocatePool2` with automatic tagged deallocation.

### 3. Defensive Buffer Validation
All user-mode virtual addresses submitted through `METHOD_NEITHER` are explicitly aligned and validated using `ProbeForRead` and `ProbeForWrite` wrapped inside Structured Exception Handling (`__try` / `__except`) to eliminate BugCheck vulnerabilities.

---

## System Requirements

| Component | Minimum Requirement | Recommended |
|---|---|---|
| Host Operating System | Windows 10 x64 (21H2+) | Windows 11 x64 (23H2+) |
| Compiler | MSVC v143 (VS 2022) or Clang-CL 17+ | Clang-CL 22+ / MSVC 19.44+ |
| Windows SDK / WDK | 10.0.22621.0 | 10.0.28000.0+ |
| Build System | CMake 3.20+ & Ninja 1.11+ | CMake 3.28+ & Ninja 1.12+ |
| Test Target | VMWare Workstation / Hyper-V (Testmode) | VMWare Workstation 17 Pro |

---

## Quickstart & Build Instructions

The project provides automated dual-compiler detection for both Microsoft Visual C++ (`cl.exe`) and LLVM Clang (`clang-cl.exe`).

### Build with Clang (Default if installed in `C:\LLVM`)
```powershell
.\build.ps1 -Compiler Clang -Config Release -Sign
```

### Build with Microsoft Visual C++
```powershell
.\build.ps1 -Compiler MSVC -Config Release -Sign
```

Output binaries are generated in `build/bin/`:
- `unpd.sys`: Digitally signed Windows kernel driver binary.
- `unpd_tests.exe`: Standalone GoogleTest validation suite.
- `unpd_test_root.cer`: Exported X.509 test root certificate.

---

## GoogleTest Validation Suite

The test suite runs in dual mode: against a live kernel driver in Windows Test Mode VMs, or in simulated loopback mode during CI runs.

| Test Suite | Test Case | Type | Expected Result |
|---|---|---|---|
| `IoctlTest` | `Ping_ValidSequence` | Functional | Roundtrip sequence validation (`Seq + 1`) |
| `IoctlTest` | `Ping_TimestampPrecision` | Telemetry | Kernel timestamp monotonically increasing |
| `IoctlTest` | `AllocateAndFree_SingleBuffer` | Memory | 1 KB NonPagedPoolNx allocation and deallocation |
| `IoctlTest` | `AllocateMultiple_CheckHandles` | Stress | 16 concurrent distinct buffer allocations |
| `IoctlTest` | `Free_InvalidHandle_ReturnsError` | Negative | Correct error returned on invalid handle |
| `IoctlTest` | `QueryStats_MetricsIntegrity` | Telemetry | Verification of global pool statistics |
| `PageEngineTest` | `MapSharedMemory_ValidAddress` | Zero-Copy | Maps 16 kernel pages (64 KB) to user space |
| `PageEngineTest` | `WriteRead_DirectSharedMemory` | Zero-Copy | Direct memory write/read without system calls |
| `PageEngineTest` | `DoubleBufferSwap_Sequence` | Concurrency | 50 atomic double-buffer index swaps |
| `PageEngineTest` | `SlabAlloc_Class64B` | Slab | 64-byte block cache allocation and free |
| `PageEngineTest` | `SlabAlloc_Class256B` | Slab | 256-byte block cache allocation and free |
| `PageEngineTest` | `SlabAlloc_Class1024B` | Slab | 1024-byte block cache allocation and free |
| `PageEngineTest` | `SlabAlloc_Class4096B` | Slab | 4096-byte page cache allocation and free |
| `FuzzingTest` | `ZeroByteAllocation_Rejected` | Boundary | Zero-byte pool request rejected |
| `FuzzingTest` | `OversizedAllocation_Rejected` | Boundary | 1 TB pool request rejected safely |
| `FuzzingTest` | `SlabAlloc_InvalidClass_Rejected` | Boundary | Invalid slab class rejected |
| `FuzzingTest` | `SlabFree_InvalidHandle_Rejected` | Boundary | Freeing null slab handle rejected |
| `FuzzingTest` | `MapSharedMemory_ZeroPages_Rejected` | Boundary | Zero-page mapping rejected |
| `FuzzingTest` | `MapSharedMemory_OversizedPages_Rejected`| Boundary | 10,000-page mapping request rejected |
| `StressTest` | `ConcurrentPing_16Threads` | Concurrency | 16 threads firing simultaneous IOCTL requests |
| `StressTest` | `ConcurrentAllocFree_8Threads` | Concurrency | 8 threads allocating and freeing pool buffers |
| `StressTest` | `ConcurrentBufferSwaps_4Threads` | Concurrency | 4 threads executing high-frequency buffer swaps |

---

## Testing in VMWare Workstation

1. Disable Secure Boot in VMWare VM Settings under Options -> Advanced.
2. Copy `build/bin/unpd.sys`, `build/bin/unpd_tests.exe`, `unpd_test_root.cer`, and `scripts/` to the VM.
3. Open an Administrator PowerShell inside the VM and run:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\Setup-VM.ps1 -CertPath .\unpd_test_root.cer
   shutdown /r /t 0
   ```
4. After rebooting into Test Mode, execute the test suite:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\Run-Tests.ps1 -SkipBuild
   ```

---

## Repository Structure

```
.
├── .github/                    # GitHub Actions CI and issue templates
│   ├── workflows/ci.yml        # Matrix build and GoogleTest validation pipeline
│   ├── ISSUE_TEMPLATE/         # Bug report and feature request templates
│   └── pull_request_template.md# Pull request review checklist
├── cmake/                      # CMake package export configurations
│   └── unpdConfig.cmake.in     # Package configuration template for find_package
├── docs/                       # Technical documentation
│   ├── ARCHITECTURE.md         # Kernel internals and memory lifecycle
│   ├── IOCTL_PROTOCOL.md       # IOCTL packet specifications
│   └── VM_SETUP.md             # VMWare testmode and WinDbg guide
├── include/unpd/               # Public and kernel headers
│   ├── common.h                # IOCTL opcodes, magic constants, and packet structs
│   ├── dispatch.hpp            # IRP dispatch declarations and device extension
│   ├── kernel_raii.hpp         # RAII primitives for kernel spinlocks and mutexes
│   ├── page_engine.hpp         # Zero-copy shared memory and slab cache interfaces
│   └── security.hpp            # User buffer validation helpers
├── ports/                      # vcpkg registry port
│   └── unsolicited-non-paged-driver/
│       ├── portfile.cmake      # vcpkg port recipe
│       └── vcpkg.json          # vcpkg package metadata
├── src/
│   ├── driver/                 # Kernel driver implementation (unpd.sys)
│   │   ├── driver_entry.cpp    # DriverEntry and deterministic teardown
│   │   ├── device_control.cpp  # IRP dispatch table router
│   │   ├── ioctl_handler.cpp   # Safe IOCTL handler logic
│   │   ├── memory_manager.cpp  # Non-paged pool tracking and handle manager
│   │   ├── page_engine.cpp     # Physical MDL mapping and atomic buffer swap
│   │   ├── kernel_asm.asm      # x64 MASM low-level hardware memory barriers
│   │   └── unpd.rc             # Windows PE version metadata
│   └── tests/                  # GoogleTest test harness (unpd_tests.exe)
│       ├── test_client.hpp     # Win32 driver client wrapper with CI mock fallback
│       ├── test_gtest_main.cpp # GTest entrypoint
│       ├── gtest_ioctl.cpp     # IOCTL functional tests
│       ├── gtest_page_engine.cpp# Shared memory and slab cache tests
│       ├── gtest_fuzzing.cpp   # Adversarial boundary and buffer fuzzing tests
│       └── gtest_stress.cpp    # Multithreaded concurrency stress tests
├── scripts/                    # Automation scripts
│   ├── bench_latency.py        # Python benchmark test runner
│   ├── Deploy-Driver.ps1       # SCM driver service manager (create, start, stop, delete)
│   ├── Run-Tests.ps1           # End-to-end test execution pipeline
│   ├── Setup-VM.ps1            # VM testmode and root certificate installer
│   └── Sign-Driver.ps1         # Certificate generation and SignTool automation
├── build.ps1                   # Dual-compiler (MSVC / Clang) 1-click build script
├── CMakeLists.txt              # CMake build configuration
├── CMakePresets.json           # Visual Studio CMake presets
├── vcpkg.json                  # vcpkg manifest
├── CODE_OF_CONDUCT.md          # Contributor Covenant Code of Conduct
├── CONTRIBUTING.md             # Contribution guidelines
├── SECURITY.md                 # Security vulnerability disclosure policy
└── LICENSE                     # MIT License
```

---

## Documentation Links

- [Kernel Architecture & Memory Model](docs/ARCHITECTURE.md)
- [IOCTL Protocol Specification](docs/IOCTL_PROTOCOL.md)
- [VMWare & WinDbg KDNET Setup Guide](docs/VM_SETUP.md)

---

## License

This project is licensed under the [MIT License](LICENSE).
