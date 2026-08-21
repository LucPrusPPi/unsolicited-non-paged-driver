<div align="center">

# Unsolicited Non-Paged Driver (UNPD)

An academic-grade, modern C++20 Windows Kernel Driver framework, universal memory manager, x86-64 MMU paging engine, and automated GoogleTest test-signing harness.

[![CI Pipeline](https://img.shields.io/badge/CI%20Matrix-6%2F6%20passing-brightgreen.svg?style=flat-square)](https://github.com/LucPrusPPi/unsolicited-non-paged-driver/actions)
[![GoogleTest](https://img.shields.io/badge/GoogleTest-118%20passed-success.svg?style=flat-square)]()
[![PyTest](https://img.shields.io/badge/PyTest-8%20passed-success.svg?style=flat-square)]()
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square)]()
[![Architecture](https://img.shields.io/badge/arch-x64%20%2F%20MASM64-orange.svg?style=flat-square)]()
[![MMU Paging](https://img.shields.io/badge/MMU-CR3%20%2F%20PML4%20%2F%20PTE-red.svg?style=flat-square)]()
[![Memory Engine](https://img.shields.io/badge/memory-MDL%20%7C%20Pool%20%7C%20Slab%20%7C%20Ring-yellow.svg?style=flat-square)]()
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
- **x86-64 MMU & Paging Engine**: Complete bitfield models for CR3, PML4, PDPTE (1GB Huge Pages), PDE (2MB Large Pages), PTE (4KB Pages), IDT, GDT, TSS64, DR0..DR7, address decomposition, PFN arithmetic, and process memory primitives (`ZwAllocateVirtualMemory`, `KeStackAttachProcess`, `Cr3Walker`, SEH probes).
- **Lockless Shared Memory Channel**: Cacheline-aligned (`alignas(64)`) ring buffers with double-buffering and MASM64 hardware serialization barriers (`UnpdFastSwapBarrier`).
- **Stealth & Diagnostic Scrubbers**: Dynamic `PiDDBCacheTable` AVL tree rebalancing, `MmUnloadedDrivers` array compaction, and `PoolBigPageTable` scrubbing.
- **Execution & Injection Primitives**: Safe Kernel APC dispatching (`KernelApc::QueueUserApc`) with automatic cleanup and rundown routines.
- **Virtual MMU Sandbox**: 64MB isolated physical RAM emulator for offline translation and `#PF` fault injection testing without kernel drivers.
- **Kernel C++20 STL (`kstd`)**: Freestanding `kstd::span<T>`, `kstd::expected<T, NTSTATUS>`, `kstd::unique_ptr<T, Tag>`, and C++20 concepts with zero CRT dependencies.
- **Public C++20 Usermode Client SDK**: High-level RAII client (`include/unpd/client.hpp`) with 16 typed methods and `SharedRingSession` RAII container with automated loopback fallback for CI environments.
- **Hardware Assembly (MASM64)**: Ring-0 memory fences, cycle counters (`rdtsc`/`rdtscp`), CR0..CR8, MSRs, DR0..DR7, descriptor tables (`sgdt`, `sidt`, `str`), hardware SSE4.2 CRC32 acceleration, and TLB invalidation (`invlpg`, `wbinvd`, CR3 reload).
- **Multi-Language Automation**: Complete mirrored scripting support in PowerShell, POSIX Shell (Git Bash/WSL), Python 3, and Lua.
- **Automated Matrix CI**: 6-job matrix in GitHub Actions (MSVC & Clang-CL in Release and Debug, Python tooling validation, schema audit) with 118 GoogleTests + 8 PyTests.

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

### 2. Hardware MMU & 4-Level x86-64 Paging (`unpd::mmu::PagingEngine` & `Cr3Walker`)
- Bitfield-accurate structures for 48-bit canonical linear addresses (`VIRTUAL_ADDRESS_64`), `CR3_REGISTER_64`, `PML4_ENTRY_64`, `PDPT_ENTRY_64` (1GB Huge Page support), `PD_ENTRY_64` (2MB Large Page support), and `PT_ENTRY_64`.
- Software page table walking from CR3 to physical PFN across user and kernel address spaces.
- Process address space switching via `ProcessAttachmentGuard` (`KeStackAttachProcess` / `KeUnstackDetachProcess`).
- Virtual page allocation and commit via `ZwAllocateVirtualMemory` / `ZwFreeVirtualMemory`.
- TLB invalidation primitives: `UnpdInvlpg` (`invlpg`), `UnpdWbinvd` (`wbinvd`), `UnpdFlushTlb`.

### 3. Lockless Shared Memory Channel (`unpd::comm::SharedMemoryChannel`)
- Cacheline-aligned (`alignas(64)`) head and tail indices eliminating false sharing across CPU cores.
- Multi-cycle lockless ring buffer supporting Ping, Stats, Swap, and memory command dispatching.
- Atomic double-buffer swap with MASM64 hardware serialization barrier (`UnpdFastSwapBarrier`).

### 4. Stealth Scrubbers & Anti-Detection Tables (`unpd::stealth`)
- **`PiDdbCleaner`**: Locates `PiDDBCacheTable` resource lock and performs in-place AVL tree node removal and height rebalancing.
- **`UnloadedCleaner`**: Compacts the circular `MmUnloadedDrivers` array and clears allocations from `PoolBigPageTable`.

### 5. Asynchronous Kernel APC Engine (`unpd::exec::KernelApc`)
- Pre-allocated, dynamically tracked `KAPC` objects targeting alertable user-mode threads.
- Custom `KernelApcCleanup` rundown routines preventing pool leaks on unhandled thread termination.

### 6. Virtual x86-64 Hardware MMU Sandbox (`unpd::test::emulator::VirtualMmu`)
- 64MB isolated physical RAM emulator.
- 4-level PML4/PDPT/PD/PT translation engine supporting 4KB, 2MB, and 1GB pages.
- Accurate `#PF` fault code generation (Not Present, Write Protect, User/Supervisor, NX, Non-Canonical).
- 64-entry LRU TLB cache model with `invlpg` and full shootdown.

### 7. Freestanding Kernel C++20 Toolkit (`kstd`)
- `kstd::span<T>`: Type-safe memory views without CRT dependencies.
- `kstd::expected<T, NTSTATUS>`: Monadic value-or-status container for exception-free error propagation.
- `kstd::unique_ptr<T, Tag>`: Automatic tagged pool cleanup on scope exit.
- `kstd` C++20 concepts: `integral`, `pointer`, `same_as`, `trivially_copyable`, `invocable`.

### 8. Usermode Client SDK (`include/unpd/client.hpp`)
- `unpd::DriverClient`: 16 typed methods wrapping all driver services (`readProcessMemoryCr3`, `writeProcessMemoryCr3`, `queueUserApc`, `cleanPiDdbCache`, `cleanUnloadedDrivers`, `mapSharedMemory`, `slabAlloc`, `queryStats`).
- `unpd::SharedRingSession`: High-level RAII session container for zero-copy memory ring buffers.
- Automated Mock Mode for offline CI environments.

---

## Complete IOCTL Opcode Index (16 Opcodes)

| Opcode Name | Code | Method | Access Mask | Purpose |
|---|---|---|---|---|
| `IOCTL_UNPD_PING` | `0x800` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Latency benchmark and protocol handshake |
| `IOCTL_UNPD_ALLOCATE_NONPAGED` | `0x801` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Allocate tracked NonPagedPoolNx block |
| `IOCTL_UNPD_FREE_NONPAGED` | `0x802` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Free tracked pool block by 64-bit handle |
| `IOCTL_UNPD_QUERY_STATS` | `0x803` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Retrieve global runtime metrics |
| `IOCTL_UNPD_PROCESS_BUFFER_DIRECT` | `0x804` | `METHOD_IN_DIRECT` | `READ \| WRITE` | High-throughput direct I/O via locked MDL |
| `IOCTL_UNPD_PROCESS_BUFFER_NEITHER` | `0x805` | `METHOD_NEITHER` | `READ \| WRITE` | Probed Neither I/O user virtual memory |
| `IOCTL_UNPD_MAP_SHARED_MEMORY` | `0x806` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Allocate physical pages & map to user VA |
| `IOCTL_UNPD_UNMAP_SHARED_MEMORY` | `0x807` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Unmap user VA and free physical pages |
| `IOCTL_UNPD_SWAP_BUFFERS` | `0x808` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Atomic lock-free double-buffer page swap |
| `IOCTL_UNPD_SLAB_ALLOC` | `0x809` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | O(1) allocation from Lookaside slab pool |
| `IOCTL_UNPD_SLAB_FREE` | `0x80A` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Return block to Lookaside slab free-list |
| `IOCTL_UNPD_READ_PROCESS_CR3` | `0x80B` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Read virtual memory of target process by CR3 |
| `IOCTL_UNPD_WRITE_PROCESS_CR3` | `0x80C` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Write virtual memory of target process by CR3 |
| `IOCTL_UNPD_QUEUE_KAPC` | `0x80D` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Queue user-mode routine via Kernel APC |
| `IOCTL_UNPD_CLEAN_PIDDB` | `0x80E` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Rebalance PiDDBCacheTable and unmap trace |
| `IOCTL_UNPD_CLEAN_UNLOADED` | `0x80F` | `METHOD_BUFFERED` | `FILE_ANY_ACCESS` | Compact MmUnloadedDrivers and clean pool record |

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

## GoogleTest Validation Suite (73 Tests Across 11 Suites)

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
| `StressTest` | `ConcurrentCr3MemoryWalk_16Threads` | Concurrency | 16 threads executing concurrent CR3 memory walks |
| `StressTest` | `ConcurrentSharedRingSession_Bursts_8Threads` | Concurrency | 8 threads sending burst packets into shared ring |
| `StressTest` | `RapidClientLifecycle_100Iterations` | Concurrency | 100 rapid cycles of client connection and teardown |
| `MmuPagingTest` | `StructureSizes_Exact64Bits` | MMU | Verifies 64-bit sizes of CR3, PML4, PDP, PD, PTE |
| `MmuPagingTest` | `VirtualAddressDecomposition_CanonicalAddress` | MMU | Decomposes canonical 48-bit address into indices |
| `MmuPagingTest` | `AlignmentHelpers_ArithmeticCorrectness` | MMU | Validates AlignUp, AlignDown, IsAligned templates |
| `MmuPagingTest` | `LargePageOffsets_2MBAnd1GB` | MMU | Validates 2MB and 1GB large page offset extraction |
| `MmuPagingTest` | `PtEntry_BitfieldsVerification` | MMU | Verifies PTE bitfields (Present, PFN, NX, RW) |
| `MmuPagingTest` | `KstdSpan_MemoryViewOperations` | Kernel STL | Validates `kstd::span` slicing and element access |
| `MmuPagingTest` | `KstdExpected_SuccessAndErrorHandling` | Kernel STL | Validates `kstd::expected` value-or-status mechanics |
| `MmuPagingTest` | `DescriptorStructures_ExactSizes` | Hardware | Verifies exact sizes of IDT, GDT, TSS64, DR7, CR0, CR4 |
| `MmuPagingTest` | `IdtEntry_OffsetPackingAndUnpacking` | Hardware | Tests 64-bit ISR address packing in IDT_ENTRY_64 |
| `MmuPagingTest` | `Dr7Register_BitfieldDecomposition` | Hardware | Validates DR7 breakpoint condition and length bitfields |
| `MmuPagingTest` | `HardwareCrc32_ComputationCorrectness` | Assembly | Validates SSE4.2 CRC32 buffer hashing against byte loop |
| `MmuPagingTest` | `AtomicBitwisePrimitives_Operations` | Assembly | Validates `lock bts`, `lock btr`, and `bt` bitwise atomics |
| `MmuPagingTest` | `HardwarePrimitives_ExtendedMasm` | Assembly | Validates MASM64 registers and segmentation routines |
| `StealthTest` | `StructureLayouts_Verification` | Stealth | Verifies size and alignment of PiDDB & Unloaded structures |
| `StealthTest` | `PatternScanner_ExactMatch` | Stealth | Validates exact signature matching in memory |
| `StealthTest` | `PatternScanner_WildcardMatch` | Stealth | Validates wildcard (`?`) pattern scanning |
| `StealthTest` | `PatternScanner_NotFound_ReturnsNull` | Stealth | Validates scanner behavior on non-matching patterns |
| `MmuAdvancedTest` | `VirtualMemoryWalkEndToEnd` | MMU | Validates full 4-level PML4 address walk |
| `MmuAdvancedTest` | `VirtualPteRemapping_PermissionsChange` | MMU | Validates PTE permission flipping (RW/NX) |
| `VirtualMmuTest` | `Basic4KbPageTranslation` | Virtual MMU | Validates 4KB page translation in physical RAM emulator |
| `VirtualMmuTest` | `Huge1GbPageTranslation` | Virtual MMU | Validates 1GB huge page address translation |
| `VirtualMmuTest` | `Large2MbPageTranslation` | Virtual MMU | Validates 2MB large page address translation |
| `VirtualMmuTest` | `PageFaultNotPresent` | Virtual MMU | Validates `#PF` fault generation when PTE is not present |
| `VirtualMmuTest` | `PageFaultWriteProtect` | Virtual MMU | Validates `#PF` fault generation on write to read-only page |
| `VirtualMmuTest` | `PageFaultNoExecute` | Virtual MMU | Validates `#PF` fault on instruction fetch with NX bit set |
| `VirtualMmuTest` | `NonCanonicalAddressFault` | Virtual MMU | Validates fault generation on non-canonical 48-bit address |
| `VirtualMmuTest` | `VirtualReadWriteMultiPageChunking` | Virtual MMU | Validates multi-page contiguous buffer chunking |
| `VirtualMmuTest` | `TlbHitAndInvlpgVerification` | Virtual MMU | Validates 64-entry LRU TLB caching and `invlpg` shootdown |
| `VirtualMmuTest` | `HigherHalfCanonicalTranslation` | Virtual MMU | Validates kernel higher-half virtual address translation |
| `VirtualMmuTest` | `UserSupervisorPrivilegeCheck` | Virtual MMU | Validates user-mode access restriction to supervisor pages |
| `VirtualMmuTest` | `MultiPageContiguousBufferChunking` | Virtual MMU | Validates cross-boundary chunked memory copying |
| `KstdTest` | `ConceptsVerification` | Kernel STL | Validates C++20 concepts (`integral`, `pointer`, `same_as`) |
| `KstdTest` | `SpanSubspanAndIterators` | Kernel STL | Validates `kstd::span` subspan slicing and iterators |
| `KstdTest` | `ExpectedSuccessAndErrorMonad` | Kernel STL | Validates `kstd::expected` monadic value semantics |
| `KstdTest` | `ExpectedVoidSpecialization` | Kernel STL | Validates `kstd::expected<void, NTSTATUS>` specialization |
| `KstdTest` | `UniquePtrRaiiManagement` | Kernel STL | Validates `kstd::unique_ptr` pool allocation and free |
| `SharedMemoryChannelTest` | `InitializeChannel_LayoutVerification` | Lockless Ring | Validates channel header, alignas(64) layout, and magics |
| `SharedMemoryChannelTest` | `SingleCommand_PushDispatchPop_Ping` | Lockless Ring | Validates push command, local dispatch, and pop response |
| `SharedMemoryChannelTest` | `RingBuffer_WrapAround_MultiCycle` | Lockless Ring | Validates multi-cycle ring buffer index wrap-around |
| `SharedMemoryChannelTest` | `DoubleBuffer_AtomicSwap_StateVerification` | Lockless Ring | Validates atomic swap of active and standby buffers |
| `SharedMemoryChannelTest` | `RingBuffer_FullCapacity_Rejection` | Lockless Ring | Validates rejection when ring buffer reaches full capacity |
| `SharedMemoryChannelTest` | `ConcurrentProducerConsumer_Stress` | Lockless Ring | Validates concurrent multithreaded push and pop |
| `ClientIntegrationTest` | `Client_ReadWriteProcessCr3_MockLoopback` | Usermode SDK | Validates CR3 memory read and write in client loopback |
| `ClientIntegrationTest` | `Client_QueueUserApc_Success` | Usermode SDK | Validates Kernel APC queuing parameters |
| `ClientIntegrationTest` | `Client_CleanPiDdbCache_ValidParams` | Usermode SDK | Validates PiDDB cache scrubber parameters |
| `ClientIntegrationTest` | `Client_CleanUnloadedDrivers_ValidParams` | Usermode SDK | Validates Unloaded drivers scrubber parameters |
| `ClientIntegrationTest` | `SharedRingSession_Lifecycle_PushPopSwap` | Usermode SDK | Validates `SharedRingSession` RAII lifecycle and swaps |
| `ClientIntegrationTest` | `SharedRingSession_MultiPacketBurst` | Usermode SDK | Validates burst packet transmission through `SharedRingSession` |

---

## Python Toolchain & Verification Suite

- **`test_python_tools.py`** (8 PyTests): Comprehensive tests verifying fuzzer logic, PE inspector, language statistics parser, and latency profiler.
- **`fuzz_runner.py`**: Adversarial IOCTL fuzzer executing 65 deterministic boundary vectors + randomized mutations.
- **`bench_latency.py`**: Sub-microsecond latency profiler calculating p50/p95/p99 execution percentiles.
- **`verify_pe.py`**: PE Authenticode, CFG, ASLR, DEP, and checksum inspector.

---

## Languages

<!-- LANGUAGES_START -->
| Language | Share | Files | Code Lines |
|---|---|---|---|
| C++ | 63.0% | 61 | 9,696 |
| Python | 15.5% | 9 | 1,394 |
| Lua |  5.4% | 7 | 797 |
| Assembly |  5.4% | 1 | 1,046 |
| PowerShell |  3.1% | 6 | 381 |
| C |  2.8% | 1 | 427 |
| Shell |  2.5% | 7 | 345 |
| CMake |  2.2% | 1 | 310 |
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
│   ├── IOCTL_PROTOCOL.md       # Complete 16-opcode IOCTL specification
│   └── VM_SETUP.md             # VMWare testmode and WinDbg guide
├── include/unpd/               # Public and kernel headers
│   ├── client.hpp              # Public C++20 Usermode Client SDK & SharedRingSession
│   ├── common.h                # 16 IOCTL opcodes, magic constants, and packet structs
│   ├── config.hpp              # Master template configuration header
│   ├── dispatch.hpp            # IRP dispatch declarations and device extension
│   ├── kernel_asm.hpp          # Ring-0 MASM64 hardware prototypes
│   ├── kernel_raii.hpp         # RAII primitives for kernel spinlocks and mutexes
│   ├── page_engine.hpp         # Zero-copy shared memory and slab cache interfaces
│   ├── security.hpp            # User buffer validation helpers
│   ├── comm/                   # Stealth Communication Subsystem
│   │   └── shared_memory.hpp   # Lockless shared memory ring & double-buffer channel
│   ├── exec/                   # Execution & Injection Primitives
│   │   └── apc.hpp             # Asynchronous Kernel APC queueing & rundown
│   ├── stealth/                # Kernel Trace Scrubbers & Stealth Engine
│   │   ├── piddb.hpp           # PiDDBCacheTable AVL tree scrubber & rebalancer
│   │   └── unloaded_drivers.hpp# MmUnloadedDrivers & PoolBigPageTable scrubber
│   ├── kstd/                   # Freestanding Kernel C++20 STL
│   │   ├── span.hpp            # Type-safe memory view span
│   │   ├── expected.hpp        # Monadic Value-or-NTSTATUS error container
│   │   ├── unique_ptr.hpp      # RAII smart pointer for tagged kernel pool
│   │   └── concepts.hpp        # Freestanding C++20 concepts
│   ├── memory/                 # Universal Multi-Strategy Memory Subsystem
│   │   └── universal_memory.hpp# MDL, System Pool, Sections, and Slab caches
│   └── mmu/                    # Hardware MMU & Paging Subsystem
│       ├── paging_types.hpp    # x86-64 CR3, PML4, PDP, PD, PTE bitfields
│       ├── paging_engine.hpp   # Page walking, process attach, virtual memory
│       ├── physical_memory.hpp # Raw PFN & MmMapIoSpaceEx physical RW
│       ├── cr3_walker.hpp      # Direct CR3 PML4 translation (no process attach)
│       ├── pte_remapper.hpp    # Direct PTE attribute manipulation (RW/NX)
│       └── descriptors.hpp     # IDT, GDT, TSS64, DR0..DR7, CR0, CR4, EFER, PAT
│
├── src/
│   ├── driver/                 # Kernel driver implementation (unpd.sys)
│   │   ├── driver_entry.cpp    # DriverEntry and deterministic teardown
│   │   ├── device_control.cpp  # 16-opcode IRP dispatch table router
│   │   ├── ioctl_handler.cpp   # Safe IOCTL handler logic with SEH probing
│   │   ├── memory_manager.cpp  # Non-paged pool tracking and handle manager
│   │   ├── page_engine.cpp     # Physical MDL mapping and atomic buffer swap
│   │   ├── kernel_asm.asm      # x64 MASM hardware memory barriers & serialization
│   │   ├── unpd.rc             # Windows PE version metadata
│   │   ├── comm/               # Shared memory channel implementation
│   │   ├── exec/               # Kernel APC injection implementation
│   │   ├── stealth/            # PiDDB and Unloaded drivers scrubbers
│   │   ├── memory/             # Universal memory implementations
│   │   └── mmu/                # Physical memory & CR3 walker implementations
│   └── tests/                  # GoogleTest test harness (unpd_tests.exe)
│       ├── main.cpp            # GoogleTest entrypoint
│       ├── test_client.hpp     # Forwards to public SDK client header
│       ├── test_ioctl.cpp      # IOCTL contract test cases
│       ├── test_page_engine.cpp# Shared memory & slab allocator tests
│       ├── test_fuzzing.cpp    # Boundary fuzzing test suite
│       ├── test_stress.cpp     # Multithreaded concurrency stress tests
│       ├── test_mmu.cpp        # MMU bitfields & x64 descriptor tests
│       ├── test_stealth.cpp    # DKOM scrubbers & table validation tests
│       ├── test_cr3_walker.cpp # Physical RAM & CR3 translation tests
│       ├── test_virtual_mmu.cpp# 64MB Virtual MMU sandbox tests
│       ├── test_kstd.cpp       # Freestanding Kernel STL tests
│       ├── test_shared_memory.cpp # Lockless shared memory channel tests
│       ├── test_client_integration.cpp # Usermode client integration tests
│       └── emulator/           # Virtual MMU sandbox implementation
│           ├── virtual_mmu.hpp # 64MB RAM emulator declaration
│           └── virtual_mmu.cpp # Page translation & #PF fault logic
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
