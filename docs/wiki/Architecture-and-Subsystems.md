# System Architecture & Subsystem Specification

## Overview

The UNPD framework is structured into decoupled, single-responsibility modules operating across three execution boundaries:
1. **Usermode Client Layer** (`unpd::client`)
2. **Ring-0 Kernel Core & Engines** (`unpd::kstd`, `unpd::mmu`, `unpd::memory`, `unpd::comm`, `unpd::exec`, `unpd::stealth`)
3. **Hardware Assembly Layer** (`MASM64`)

---

## Subsystem Registry

### 1. `unpd::kstd` (Freestanding Kernel Standard Library)
Provides ISO C++20 standard library equivalents optimized for kernel mode execution without C-Runtime (`msvcrt`) link dependencies.
- **`kstd::span<T>`**: Bounds-checked view of contiguous memory.
- **`kstd::expected<T, E>`**: Monadic error handling replacing C++ exception handling.
- **`kstd::unique_ptr<T, Tag>`**: RAII smart pointer wrapping `ExFreePoolWithTag`.
- **C++20 Concepts**: `integral`, `pointer`, `same_as`, `trivially_copyable`, `invocable`.

### 2. `unpd::mmu` (Memory Management Unit & Paging)
Direct hardware translation and manipulation of virtual and physical address spaces.
- **`Cr3Walker`**: 4-level PML4 page table walk without process attachment.
- **`PhysicalMemoryMapping<T>`**: RAII wrapper over `MmMapIoSpace` and `MmUnmapIoSpace`.
- **`ProcessAttachmentGuard`**: RAII guard for safe cross-process unmapping via `KeStackAttachProcess`.

### 3. `unpd::memory` (Polymorphic Memory Engine)
Abstract strategy pattern (`IMemoryEngine`) managing diverse allocation mechanisms:
- **`MdlMemoryEngine`**: Zero-copy physical page allocations mapped to user space with `MdlMappingNoExecute`.
- **`SlabMemoryEngine`**: O(1) Lookaside lists (`NPAGED_LOOKASIDE_LIST`) for 64B, 256B, 1024B, and 4096B blocks.
- **`PoolMemoryEngine`**: Tracked `NonPagedPoolNx` allocations with dynamic 64-bit handle indexing.
- **`DirectNeitherEngine`**: Probed user pointer transfers in Structured Exception Handling.

### 4. `unpd::comm` (Lockless Shared Memory Channel)
High-performance inter-process communication backend.
- **`SharedMemoryChannel`**: Ring buffer layout with `alignas(64)` cacheline padding.
- **Atomic Double Buffering**: Active/Standby buffer swapping with MASM64 `mfence` memory barriers.

### 5. `unpd::exec` (Kernel APC Dispatcher)
- **`KernelApc`**: Asynchronous User APC queueing using `KeInitializeApc` and `KeInsertQueueApc`.
- Implements `KernelApcRundown` and `KernelApcCleanup` handlers to guarantee pool deallocation on thread exit.

### 6. `unpd::stealth` (Kernel Trace Scrubbers)
- **`PiDdbCleaner`**: Safe AVL tree node traversal and deletion on `PiDDBCacheTable`.
- **`UnloadedCleaner`**: Compaction of `MmUnloadedDrivers` circular arrays and `PoolBigPageTable` validation.

### 7. `unpd::client` (Usermode Client SDK)
- **`DriverClient`**: 16 strongly typed C++20 methods wrapping driver IOCTL services.
- **`SharedRingSession`**: RAII session container for zero-copy memory ring operations with Mock Loopback support.

### 8. `unpd::test::emulator` (Virtual MMU Sandbox)
- **`VirtualMmu`**: 64MB simulated physical RAM environment with PML4 translation, 64-entry LRU TLB, and `#PF` fault generation.

---

## Data Flow Model

```
+-------------------------------------------------------------------+
|                        Usermode Application                       |
|                   [ DriverClient / SharedRingSession ]            |
+-------------------------------------------------------------------+
           |                                        |
           | DeviceIoControl (Buffered / Neither)   | Zero-Copy Shared Ring
           v                                        v
+-------------------------------------------------------------------+
|                     Ring-0 IRP Dispatch Router                    |
|                    [ src/driver/ioctl_handler.cpp ]               |
+-------------------------------------------------------------------+
           |                                        |
           v                                        v
+-----------------------+                +--------------------------+
|  Polymorphic Memory   |                |  4-Level MMU CR3 Walker  |
|  [ IMemoryEngine ]    |                |  [ Cr3Walker ]           |
+-----------------------+                +--------------------------+
           |                                        |
           +-------------------+--------------------+
                               |
                               v
+-------------------------------------------------------------------+
|                   MASM64 Hardware Assembly Layer                  |
|          [ CR0..CR8, DR0..DR7, invlpg, wbinvd, mfence, CRC32 ]     |
+-------------------------------------------------------------------+
```
