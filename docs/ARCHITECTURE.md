# UNPD Kernel Architecture & Design Specification

## 1. Overview & Architectural Goals

The **Unsolicited Non-Paged Driver (UNPD)** framework is designed around deterministic memory management, zero-copy high-throughput communication, and hardware-level transparency in Windows NT kernel space. It serves as an academic and production-grade template for 64-bit Windows driver development.

```
 +-------------------------------------------------------------------------+
 |                      Usermode Client / Application                      |
 |                      [ include/unpd/client.hpp ]                        |
 +-------------------------------------------------------------------------+
       | (Mapped User VA)                            | (Win32 DeviceIoControl)
       |                                             |
       v                                             v
 +----------------------------+              +-----------------------------+
 |  Zero-Copy Shared Pages    |              | \\.\UnsolicitedNonPagedDriver|
 |  - Buffer A (Active)       |              +-----------------------------+
 |  - Buffer B (Standby)      |                             |
 +----------------------------+                             v
       ^                                     +-----------------------------+
       | (Atomic Swap & TLB Invalidate)      |   IRP Dispatch Router       |
       |                                     |   - Type-safe dispatch      |
 +----------------------------+              |   - SEH Pointer Validation  |
 |  Universal Memory Manager  |              +-----------------------------+
 |  - MmAllocatePagesForMdlEx |                             |
 |  - ExAllocatePool2 ('UNPD')|                             v
 |  - Lookaside Slab Caches   |              +-----------------------------+
 |  - Named Shared Sections   |              |  MMU & Process Memory Engine|
 +----------------------------+              |  - 4-Level Page Table Walk  |
       |                                     |  - Process Attach / Detach  |
       v                                     |  - CR3, PML4, PDP, PD, PTE  |
 +------------------------------------+      +-----------------------------+
 |     Hardware Assembly Layer        |                     |
 |     - MASM64 Serialization Fences  |<--------------------+
 |     - rdtsc / rdtscp / invlpg      |
 +------------------------------------+
```

---

## 2. Driver Lifecycle and Teardown Sequence

```
[ OS Kernel Loader ] 
        |
        v
  DriverEntry()
        |---> UniversalMemoryManager::Initialize() (Setup Lookaside lists)
        |---> IoCreateDevice(FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN)
        |---> Initialize UNPD_DEVICE_EXTENSION (Spinlocks, Allocation Table, Metrics)
        |---> IoCreateSymbolicLink(\\DosDevices\\UnsolicitedNonPagedDriver)
        |---> Register MajorFunction[IRP_MJ_CREATE, IRP_MJ_CLOSE, IRP_MJ_DEVICE_CONTROL]
        |---> Clear DO_DEVICE_INITIALIZING
        v
  [ Active Serving State ] <---> User-Mode IOCTLs & Zero-Copy Streams
        |
        v (Service Stop / Driver Unload)
  DriverUnload()
        |---> IoDeleteSymbolicLink(\\DosDevices\\UnsolicitedNonPagedDriver)
        |---> UniversalMemoryManager::Shutdown() (Unmap MDLs, Delete Lookasides)
        |---> Acquire StateLock (KSPIN_LOCK)
        |---> Free all tracked NonPagedPoolNx allocations
        |---> Release StateLock
        |---> IoDeleteDevice()
        v
[ Driver Terminated Cleanly (Zero Leaks) ]
```

---

## 3. Universal Multi-Strategy Memory Subsystem

The driver supports five distinct memory management strategies via `unpd::memory::UniversalMemoryManager`:

### 1. Physical MDL Zero-Copy Mapping (`PhysicalMdlZeroCopy`)
- Allocates contiguous physical RAM pages via `MmAllocatePagesForMdlEx` using `MM_ALLOCATE_PREFER_CONTIGUOUS`.
- Maps locked pages directly into user-mode address space using `MmMapLockedPagesSpecifyCache` with `UserMode` and `MdlMappingNoExecute` (W^X DEP enforcement).
- Implements atomic lockless double-buffering page swaps using `InterlockedExchange` and `UnpdMemoryFence()`.

### 2. Tracked Non-Paged System Pool (`SystemPoolNonPaged`)
- Allocates non-executable non-paged memory using `ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'DPNU')`.
- All allocations are registered with 64-bit opaque handles in an intrusive doubly-linked list (`LIST_ENTRY`), protected by `devExt->StateLock`.

### 3. Fixed-Size Lookaside Slab Cache (`SlabCachePool`)
- Implements four high-speed cache tiers (64B, 256B, 1024B, 4096B) backed by `NPAGED_LOOKASIDE_LIST`.
- Allocations and deallocations execute in O(1) time without taking global pool locks, eliminating pool fragmentation.

### 4. Kernel Section Shared Memory (`KernelSectionShared`)
- Creates Section objects (`ZwCreateSection` / `ZwMapViewOfSection`) allowing structured shared memory windows across distinct processes.

### 5. Direct and Probed User Virtual Buffers (`DirectNeitherBuffer`)
- Validates user-mode virtual addresses (`METHOD_NEITHER`) using `ProbeForRead` and `ProbeForWrite` wrapped in dedicated Structured Exception Handling (`__try` / `__except`) functions to guarantee zero BugCheck crashes.

---

## 4. Hardware MMU & 4-Level x86-64 Paging Engine

The driver includes complete bitfield definitions and software walking routines for the x86-64 Long Mode MMU architecture in [include/unpd/mmu/paging_types.hpp](file:///e:/FastFarmer/Unsolicited%20Non-Paged%20Driver/include/unpd/mmu/paging_types.hpp):

### 48-bit Canonical Linear Address Decomposition
A 64-bit virtual address is partitioned into:
- `Offset4KB` (Bits 0..11): Byte offset within 4KB physical page.
- `PtIndex` (Bits 12..20): Index into Page Table (PTE, 512 entries).
- `PdIndex` (Bits 21..29): Index into Page Directory (PDE, 512 entries).
- `PdptIndex` (Bits 30..38): Index into Page Directory Pointer Table (PDPTE, 512 entries).
- `Pml4Index` (Bits 39..47): Index into Page Map Level 4 (PML4E, 512 entries).
- `SignExtension` (Bits 48..63): Must match Bit 47 for canonical compliance.

### Hardware Page Table Entries
- `CR3_REGISTER_64`: PML4 physical base address, PCID (Process Context Identifier), PWT, PCD.
- `PML4_ENTRY_64`, `PDPT_ENTRY_64`, `PD_ENTRY_64`, `PT_ENTRY_64`: Bitfields for Present, Writable, User/Supervisor, WriteThrough, CacheDisable, Accessed, Dirty, LargePage (1GB/2MB), Global, PAT, PageFrameNumber (PFN), and ExecuteDisable (NX).

### Process Address Space Operations (`unpd::mmu::PagingEngine`)
- `ProcessAttachmentGuard`: RAII switcher for process address spaces (`KeStackAttachProcess` / `KeUnstackDetachProcess`) with APC state preservation.
- `AllocateProcessMemory` / `FreeProcessMemory`: Allocates committed virtual pages inside a target process (`ZwAllocateVirtualMemory` / `ZwFreeVirtualMemory`).
- `SafeCopyProcessMemory`: Isolated memory transfers between distinct processes with intermediate kernel buffering and SEH verification.
- `InvalidatePage` (`invlpg`) & `FlushCoreTlb`: Targeted single-page TLB invalidation and full CR3 reload.

---

## 5. Kernel C++20 Standard Toolkit (`kstd`)

Because Microsoft's Standard C++ Library (`<vector>`, `<memory>`, `<expected>`) cannot be used in freestanding `/kernel` mode, UNPD provides its own zero-overhead, exception-free Kernel STL:

- `unpd::kstd::span<T>`: Bounds-checked, type-safe contiguous memory view.
- `unpd::kstd::expected<T, NTSTATUS>`: Value-or-NTSTATUS container for clean, exception-free error propagation.
- `unpd::kstd::unique_ptr<T, Tag>`: RAII smart pointer wrapping kernel pool allocations with automatic tagged deallocation.

---

## 6. MASM64 Ring-0 Assembly Layer

Low-level hardware routines are implemented in [src/driver/kernel_asm.asm](file:///e:/FastFarmer/Unsolicited%20Non-Paged%20Driver/src/driver/kernel_asm.asm):
- `UnpdMemoryFence`, `UnpdLoadFence`, `UnpdStoreFence`: Hardware memory barriers (`mfence`, `lfence`, `sfence`).
- `UnpdReadTsc`, `UnpdReadTscp`: High-resolution cycle counting with serialization.
- `UnpdFastCopy64`, `UnpdFastZero64`: High-throughput 64-bit block streaming (`rep movsq`, `rep stosq`).
- `UnpdReadCr0`..`UnpdReadCr4`, `UnpdWriteCr0`..`UnpdWriteCr4`: Control register access.
- `UnpdInvlpg`, `UnpdWbinvd`, `UnpdFlushTlb`: TLB and cache management.
- `UnpdReadMsr`, `UnpdWriteMsr`: Direct Model-Specific Register read/write.
