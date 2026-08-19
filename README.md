<div align="center">

# Unsolicited Non-Paged Driver (UNPD)

An academic-grade, modern C++20 Windows Kernel Driver framework, universal memory manager, x86-64 MMU paging engine, and automated GoogleTest test-signing harness.

[![CI Pipeline](https://img.shields.io/badge/CI%20Matrix-6%2F6%20passing-brightgreen.svg?style=flat-square)](https://github.com/LucPrusPPi/unsolicited-non-paged-driver/actions)
[![GoogleTest](https://img.shields.io/badge/tests-34%20passed-success.svg?style=flat-square)]()
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square)]()
[![Architecture](https://img.shields.io/badge/arch-x64%20%2F%20MASM64-orange.svg?style=flat-square)]()
[![MMU Paging](https://img.shields.io/badge/MMU-CR3%20%2F%20PML4%20%2F%20PTE-red.svg?style=flat-square)]()
[![Memory Engine](https://img.shields.io/badge/memory-MDL%20%7C%20Pool%20%7C%20Slab%20%7C%20Section-yellow.svg?style=flat-square)]()
[![Scripting](https://img.shields.io/badge/scripting-Python%20%7C%20Shell%20%7C%20Lua-blueviolet.svg?style=flat-square)]()
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20x64-informational.svg?style=flat-square)]()
[![Package Manager](https://img.shields.io/badge/vcpkg-supported-purple.svg?style=flat-square)](https://github.com/LucPrusPPi/unsolicited-non-paged-driver)
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)](LICENSE)

</div>

---

## Overview and Motivation

Windows kernel driver development has historically suffered from fragmented C-style boilerplates, error-prone manual memory deallocation sequences, and fragile testing workflows. UNPD establishes an extensible, modern, and idiomatically structured template for 64-bit Windows NT driver engineering.

Built from the ground up for Windows 10 and Windows 11 x64, UNPD provides:
- **Modular Universal Memory**: Physical zero-copy MDL mapping, tracked NonPagedPoolNx, Lookaside slab pools (64B..4KB), and named shared section mapping.
- **x86-64 MMU & Paging Engine**: Complete bitfield models for CR3, PML4, PDPTE, PDE, PTE, IDT, GDT, TSS64, DR0..DR7, address decomposition, PFN arithmetic, and process memory primitives (`ZwAllocateVirtualMemory`, `KeStackAttachProcess`, SEH probes).
- **Kernel C++20 STL (`kstd`)**: Freestanding `kstd::span<T>`, `kstd::expected<T, NTSTATUS>`, and `kstd::unique_ptr<T, Tag>`.
- **Public C++20 Usermode Client SDK**: High-level RAII client (`include/unpd/client.hpp`) with automated loopback fallback for CI environments.
- **Hardware Assembly (MASM64)**: Ring-0 memory fences, cycle counters (`rdtsc`/`rdtscp`), CR0..CR8, MSRs, DR0..DR7, descriptor tables (`sgdt`, `sidt`, `str`), hardware SSE4.2 CRC32 acceleration, and TLB invalidation (`invlpg`, `wbinvd`, CR3 reload).
- **Multi-Language Automation**: Complete mirrored scripting support in PowerShell, POSIX Shell (Git Bash/WSL), Python 3, and Lua.
- **Automated Matrix CI**: 6-job matrix in GitHub Actions (MSVC & Clang-CL in Release and Debug, Python tooling validation, schema audit) with 34 automated tests.

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
 |  - RAII Spinlocks / Mutexes |     |    Universal Memory Manager   |
 |  - MASM64 Memory Barriers   |     |    - Non-Paged Pool (ExAlloc2)|
 |  - MMU 4-Level Page Walking |     |    - Lookaside Lists          |
 |  - Hardware SSE4.2 CRC32    |     +-------------------------------+
 +-----------------------------+
```

---

## Key Technical Subsystems

### 1. Universal Memory Manager (`unpd::memory::UniversalMemoryManager`)
- **`PhysicalMdlZeroCopy`**: Physical RAM allocation with zero-copy user-space mapping (`MmMapLockedPagesSpecifyCache` with `MdlMappingNoExecute`).
- **`SystemPoolNonPaged`**: Tagged NonPagedPoolNx allocations with dynamic handle tracking.
- **`SlabCachePool`**: O(1) Lookaside lists for 64B, 256B, 1024B, and 4096B block caches.
- **`KernelSectionShared`**: Cross-process and user-kernel shared sections.
- **`DirectNeitherBuffer`**: Safe user-mode pointer validation with SEH isolation.

### 2. Hardware MMU & 4-Level x86-64 Paging (`unpd::mmu::PagingEngine`)
- Bitfield-accurate structures for 48-bit canonical linear addresses (`VIRTUAL_ADDRESS_64`), `CR3_REGISTER_64`, `PML4_ENTRY_64`, `PDPT_ENTRY_64` (1GB Huge Page support), `PD_ENTRY_64` (2MB Large Page support), and `PT_ENTRY_64`.
- Software page table walking from CR3 to physical PFN.
- Process address space switching via `ProcessAttachmentGuard` (`KeStackAttachProcess` / `KeUnstackDetachProcess`).
- Virtual page allocation and commit via `ZwAllocateVirtualMemory` / `ZwFreeVirtualMemory`.
- TLB invalidation primitives: `UnpdInvlpg` (`invlpg`), `UnpdWbinvd` (`wbinvd`), `UnpdFlushTlb`.

### 3. Descriptor Tables & Hardware Assembly Primitives (MASM64)
- Descriptor tables: `DESCRIPTOR_TABLE_REGISTER_64`, `IDT_ENTRY_64`, `GDT_ENTRY_64`, `TSS64`.
- Hardware Debug Registers: `DR0..DR3`, `DR6`, `DR7_REGISTER_64` (Execution, Write, I/O watchpoints).
- Extended registers: `CR8` (Task Priority Register), `XCR0` (AVX/XSAVE state), `IA32_EFER`, `IA32_PAT`.
- Hardware SSE4.2 CRC32 memory streaming (`UnpdComputeCrc32_Buffer`) and atomic bit operations (`bts`, `btr`, `bt`).

### 4. Freestanding Kernel C++20 Toolkit (`kstd`)
- `kstd::span<T>`: Type-safe memory views without CRT dependencies.
- `kstd::expected<T, NTSTATUS>`: Value-or-status container for exception-free error propagation.
- `kstd::unique_ptr<T, Tag>`: Automatic tagged pool cleanup on scope exit.

### 5. Usermode Client SDK (`include/unpd/client.hpp`)
- `unpd::DriverClient`: Connects to `\\.\UnsolicitedNonPagedDriver`, manages shared memory sessions, atomic buffer swapping, and slab allocations with automatic mock fallback for non-kernel test environments.

---

## Quickstart & Build Instructions

### Build with PowerShell (Windows)
```powershell
.\build.ps1 -Compiler Auto -Config Release -Sign
```

### Build with Shell (Git Bash / WSL)
```bash
./build.sh --config Release --sign
```

### 1-Click Template Customizer (Rebranding)
To instantiate this template for a new driver project:
```bash
python scripts/python/init_template.py --name "MyHypervisorDriver" --tag "HYPR"
```

---

## GoogleTest Validation Suite (34 Tests)

| Test Suite | Test Case | Type | Expected Result |
|---|---|---|---|
| `IoctlTest` | `Ping_ValidSequence` | Functional | Roundtrip sequence validation (`Seq + 1`) |
| `IoctlTest` | `Ping_TimestampPrecision` | Telemetry | Monotonically increasing kernel timestamp |
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
| `MmuPagingTest` | `StructureSizes_Exact64Bits` | MMU | Verifies 64-bit sizes of CR3, PML4, PDP, PD, PTE |
| `MmuPagingTest` | `VirtualAddressDecomposition_CanonicalAddress` | MMU | Decomposes canonical 48-bit address into indices |
| `MmuPagingTest` | `AlignmentHelpers_ArithmeticCorrectness` | MMU | Validates AlignUp, AlignDown, IsAligned templates |
| `MmuPagingTest` | `LargePageOffsets_2MBAnd1GB` | MMU | Validates 2MB and 1GB large page offset extraction |
| `MmuPagingTest` | `PtEntry_BitfieldsVerification` | MMU | Verifies PTE bitfields (Present, PFN, NX, RW) |
| `MmuPagingTest` | `KstdSpan_MemoryViewOperations` | Kernel STL | Validates kstd::span slicing and element access |
| `MmuPagingTest` | `KstdExpected_SuccessAndErrorHandling` | Kernel STL | Validates kstd::expected value-or-status mechanics |
| `MmuPagingTest` | `DescriptorStructures_ExactSizes` | Hardware | Verifies exact sizes of IDT, GDT, TSS64, DR7, CR0, CR4 |
| `MmuPagingTest` | `IdtEntry_OffsetPackingAndUnpacking` | Hardware | Tests 64-bit ISR address packing in IDT_ENTRY_64 |
| `MmuPagingTest` | `Dr7Register_BitfieldDecomposition` | Hardware | Validates DR7 breakpoint condition and length bitfields |
| `MmuPagingTest` | `HardwareCrc32_ComputationCorrectness` | Assembly | Validates SSE4.2 CRC32 buffer hashing against byte loop |
| `MmuPagingTest` | `AtomicBitwisePrimitives_Operations` | Assembly | Validates lock bts, lock btr, and bt bitwise atomics |

---

## Languages

<!-- LANGUAGES_START -->
| Language | Share | Files | Code Lines |
|---|---|---|---|
| C++ | 61.6% | 48 | 5,772 |
| Lua |  9.4% | 7 | 797 |
| Python |  8.5% | 7 | 687 |
| Assembly |  5.5% | 1 | 624 |
| PowerShell |  4.6% | 5 | 324 |
| Shell |  4.3% | 7 | 345 |
| CMake |  3.4% | 1 | 286 |
| C |  2.7% | 1 | 244 |
<!-- LANGUAGES_END -->

---

## Repository Structure

```
.
├── .github/workflows/          # GitHub Actions CI matrix
│   └── ci.yml                  # 6-job matrix pipeline (MSVC/Clang-CL/Python/Audit)
├── cmake/                      # CMake package export configurations
│   └── unpdConfig.cmake.in     # Package configuration template for find_package
├── docs/                       # Technical documentation
│   ├── ARCHITECTURE.md         # Kernel internals, MMU paging, and memory lifecycle
│   ├── IOCTL_PROTOCOL.md       # Complete 11-opcode IOCTL specification
│   └── VM_SETUP.md             # VMWare testmode and WinDbg guide
├── include/unpd/               # Public and kernel headers
│   ├── client.hpp              # Public C++20 Usermode Client SDK
│   ├── common.h                # IOCTL opcodes, magic constants, and packet structs
│   ├── config.hpp              # Master template configuration header
│   ├── dispatch.hpp            # IRP dispatch declarations and device extension
│   ├── kernel_asm.hpp          # Ring-0 MASM64 hardware prototypes
│   ├── kernel_raii.hpp         # RAII primitives for kernel spinlocks and mutexes
│   ├── page_engine.hpp         # Zero-copy shared memory and slab cache interfaces
│   ├── security.hpp            # User buffer validation helpers
│   ├── comm/                   # Stealth Communication Subsystem
│   │   └── backend_shared_mem.hpp # Lockless shared memory polling backend
│   ├── exec/                   # Execution & Injection Primitives
│   │   └── kernel_apc.hpp      # Asynchronous Kernel APC queueing
│   ├── stealth/                # Kernel Trace Scrubbers & Stealth Engine
│   │   ├── piddb_cleaner.hpp   # PiDDBCacheTable & KernelHashBucketList scrubber
│   │   └── unloaded_cleaner.hpp# MmUnloadedDrivers & PoolBigPageTable scrubber
│   ├── kstd/                   # Freestanding Kernel C++20 STL
│   │   ├── kstd_span.hpp       # Type-safe memory view span
│   │   ├── kstd_expected.hpp   # Value-or-NTSTATUS error container
│   │   └── kstd_unique_ptr.hpp # RAII smart pointer for tagged kernel pool
│   ├── memory/                 # Universal Multi-Strategy Memory Subsystem
│   │   └── universal_memory.hpp# MDL, System Pool, Sections, and Slab caches
│   └── mmu/                    # Hardware MMU & Paging Subsystem
│       ├── paging_types.hpp    # x86-64 CR3, PML4, PDP, PD, PTE bitfields
│       ├── paging_engine.hpp   # Page walking, process attach, virtual memory
│       ├── physical_memory.hpp # Raw PFN & MmMapIoSpaceEx physical RW
│       ├── cr3_walker.hpp      # Direct CR3 PML4 translation (no process attach)
│       ├── pte_remapper.hpp    # Direct PTE attribute manipulation (RW/NX)
│       └── descriptors.hpp     # IDT, GDT, TSS64, DR0..DR7, CR0, CR4, EFER, PAT

├── src/
│   ├── driver/                 # Kernel driver implementation (unpd.sys)
│   │   ├── driver_entry.cpp    # DriverEntry and deterministic teardown
│   │   ├── device_control.cpp  # IRP dispatch table router
│   │   ├── ioctl_handler.cpp   # Safe IOCTL handler logic
│   │   ├── memory_manager.cpp  # Non-paged pool tracking and handle manager
│   │   ├── page_engine.cpp     # Physical MDL mapping and atomic buffer swap
│   │   ├── kernel_asm.asm      # x64 MASM low-level hardware memory barriers & MMU
│   │   ├── unpd.rc             # Windows PE version metadata
│   │   ├── comm/               # Shared memory backend implementation
│   │   ├── exec/               # Kernel APC injection implementation
│   │   ├── stealth/            # PiDDB and Unloaded drivers scrubbers
│   │   ├── memory/             # Universal memory implementations
│   │   └── mmu/                # Physical memory & CR3 walker implementations
│   └── tests/                  # GoogleTest test harness (unpd_tests.exe)
│       ├── test_client.hpp     # Forwards to public SDK client header
│       ├── test_gtest_main.cpp # GTest entrypoint
│       ├── gtest_ioctl.cpp     # IOCTL functional tests
│       ├── gtest_page_engine.cpp# Shared memory and slab cache tests
│       ├── gtest_fuzzing.cpp   # Adversarial boundary and buffer fuzzing tests
│       ├── gtest_stress.cpp    # Multithreaded concurrency stress tests
│       ├── gtest_mmu.cpp       # MMU bitfields, virtual address & kstd tests
│       ├── gtest_stealth.cpp   # PiDDB & Unloaded drivers scrubber tests
│       └── gtest_cr3_walker.cpp# Direct CR3 walker & physical memory tests
├── scripts/                    # Automation and tooling scripts by language
│   ├── bash/                   # POSIX Shell & Git Bash automation
│   │   ├── Deploy-Driver.sh    # SCM driver service manager
│   │   ├── Run-Tests.sh        # End-to-end test execution pipeline
│   │   ├── Setup-VM.sh         # VM testmode and root certificate installer
│   │   ├── Sign-Driver.sh      # Certificate generation and SignTool
│   │   ├── ci_check.sh         # Repository integrity and token audit
│   │   └── package_release.sh  # Artifact packaging and SHA256 generator
│   ├── powershell/             # Windows PowerShell automation
│   │   ├── Deploy-Driver.ps1   # SCM driver service manager
│   │   ├── Run-Tests.ps1       # End-to-end test execution pipeline
│   │   ├── Setup-VM.ps1        # VM testmode and root certificate installer
│   │   └── Sign-Driver.ps1     # Certificate generation and SignTool
│   ├── python/                 # Python 3 toolchain and fuzzer
│   │   ├── bench_latency.py    # Python latency percentile profiler (p50/p95/p99)
│   │   ├── driver_ctl.py       # Win32 SCM service manager via ctypes
│   │   ├── fuzz_runner.py      # Boundary and randomized kernel IOCTL fuzzer
│   │   ├── init_template.py    # 1-click template customizer and project renamer
│   │   ├── update_readme_stats.py # Automated repository language statistics updater
│   │   └── verify_pe.py        # PE Authenticode, CFG, ASLR, and DEP inspector
│   └── lua/                    # Lua automation, simulation & testing engine
│       ├── benchmark_suite.lua # Latency percentile profiler & telemetry
│       ├── config_validator.lua# Template schema & parameter validator
│       ├── driver_test.lua     # IOCTL protocol automation engine
│       ├── fuzz_scenario.lua   # Mutation generator & fuzz scenarios
│       ├── mmu_engine.lua      # 4-level x86-64 MMU page table simulator
│       ├── protocol_codec.lua  # Binary IOCTL packet codec & dissector
│       └── stateful_fuzzer.lua # Stateful kernel workflow sequence fuzzer
├── build.ps1                   # Dual-compiler (MSVC / Clang) build script (PowerShell)
├── build.sh                    # Dual-compiler build runner (POSIX Shell / Git Bash)
├── CMakeLists.txt              # CMake build configuration
├── vcpkg.json                  # vcpkg manifest
├── LICENSE                     # MIT License
└── SECURITY.md                 # Security disclosure policy
```

---

## License

This project is licensed under the [MIT License](LICENSE).
