# UNPD Kernel Architecture & Design Specification

## 1. Overview & Architectural Goals

The **Unsolicited Non-Paged Driver (UNPD)** framework is designed around deterministic memory management, zero-copy high-throughput communication, hardware MMU transparency, and exception-free C++20 design in Windows NT kernel space. It serves as an extensible, academic-grade template for 64-bit Windows driver development.

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
 |  Polymorphic Memory Engine |              +-----------------------------+
 |  [ IMemoryEngine ]         |                             |
 |  ├── MdlMemoryEngine       |                             v
 |  ├── SlabMemoryEngine      |              +-----------------------------+
 |  ├── PoolMemoryEngine      |              |  MMU & Process Memory Engine|
 |  └── DirectNeitherEngine   |              |  - 4-Level Page Table Walk  |
 +----------------------------+              |  - Process Attach / Detach  |
       |                                     |  - CR3, PML4, PDP, PD, PTE  |
       v                                     +-----------------------------+
 +------------------------------------+                     |
 |     Hardware Assembly Layer        |                     |
 |     - MASM64 Serialization Fences  |<--------------------+
 |     - rdtsc / rdtscp / invlpg      |
 +------------------------------------+
```

---

## 2. Driver Lifecycle & Teardown Invariants

```
[ OS Kernel Loader ] 
        |
        v
  DriverEntry()
        |---> UniversalMemoryManager::Initialize() (Polymorphic Engines)
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
[ Driver Terminated Cleanly (Zero Leaks, Zero Dangling PFNs) ]
```

---

## 3. Deep Dive: Memory Descriptor List (MDL) Architecture & Usage

A **Memory Descriptor List (MDL)** is the Windows NT kernel's native structure for describing physical memory pages backing a contiguous virtual address range or an allocated physical page set.

### 3.1 Layout of the `MDL` Structure
In the NT kernel, `MDL` is defined as:
```c
typedef struct _MDL {
    struct _MDL *Next;
    CSHORT Size;
    CSHORT MdlFlags;
    struct _EPROCESS *Process;
    PVOID MappedSystemVa;
    PVOID StartVa;
    ULONG ByteCount;
    ULONG ByteOffset;
    // Followed immediately in memory by an array of physical page frame numbers:
    // PFN_NUMBER PfnArray[ (ByteCount + ByteOffset + PAGE_SIZE - 1) / PAGE_SIZE ];
} MDL, *PMDL;
```

### 3.2 Complete MDL Lifecycle in UNPD (`MdlMemoryEngine`)

```
   1. Physical Frame Allocation (No Virtual Address Consumed)
      MmAllocatePagesForMdlEx(low, high, skip, bytes, MmCached, MM_ALLOCATE_PREFER_CONTIGUOUS)
                                  |
                                  v  Returns PMDL describing physical PFN array
   2. Zero-Copy User Space Mapping
      MmMapLockedPagesSpecifyCache(mdl, UserMode, MmCached, NULL, FALSE, NormalPagePriority | MdlMappingNoExecute)
                                  |
                                  v  Allocates user PTEs (0x000000000000..0x7FFFFFFFFFFF)
   3. Lock-Free Double Buffering & Atomic Swap
      User writes to Active Buffer -> InterlockedExchange(ActiveIndex) -> UnpdMemoryFence()
                                  |
                                  v
   4. Deterministic Cleanup on Session Close / Driver Unload
      MmUnmapLockedPages(userVa, mdl)  [Must execute in owning process context]
      MmFreePagesFromMdl(mdl)          [Releases physical frames to OS Free PFN list]
      IoFreeMdl(mdl)                   [Frees the MDL header itself]
```

### 3.3 Why MDL Zero-Copy Outperforms Buffered & Direct I/O
1. **Zero System Call Overhead**: Once mapped, data streams directly through memory without issuing `DeviceIoControl` syscalls.
2. **Zero Kernel Virtual Address (KVA) Consumption**: Pages are allocated directly from physical memory and mapped into User VA without consuming scarce NonPaged Pool or System PTEs.
3. **Hardware DEP (Data Execution Prevention) Compliance**: Passing `MdlMappingNoExecute` guarantees that mapped pages have the NX bit set in their PTEs, preventing arbitrary code execution.
4. **PFN Database Integrity**: Unlike manual PTE manipulation, `MmAllocatePagesForMdlEx` properly increments PFN reference counts in `nt!MmPfnDatabase`, ensuring zero `0x4E: PFN_LIST_CORRUPT` bugchecks during process exit or memory trimming.

---

## 4. Deep Dive: x86-64 CR3 Architecture & MMU Paging

The x86-64 processor MMU uses the **CR3 Control Register** (also known as the Page Directory Base Register - PDBR) as the hardware root for 4-level linear address translation.

### 4.1 CR3 Register Format & Flags
```
 63                                  52 51                               12 11        5 4   3 2      0
+--------------------------------------+-----------------------------------+-----------+---+---+------+
|               Reserved               |    PML4 Physical Base Address     |  Reserved |PCD|PWT| PCID |
|                (MBZ)                 |            (Bits 51..12)          |   (MBZ)   |   |   |(0..11|
+--------------------------------------+-----------------------------------+-----------+---+---+------+
```
- **PML4 Physical Base Address (Bits 12..51)**: 4KB-aligned physical memory address pointing to the base of the Page Map Level 4 table (512 entries x 8 bytes = 4096 bytes).
- **PCID (Process Context Identifier, Bits 0..11)**: When enabled in CR4 (`CR4.PCIDE = 1`), allows the CPU TLB to tag translations per process, preventing complete TLB flushes across context switches.
- **PWT (Page-level Write-Through, Bit 3)**: Enables write-through caching for the PML4 table.
- **PCD (Page-level Cache Disable, Bit 4)**: Disables processor caching for PML4 memory lookups.

### 4.2 4-Level Canonical Virtual Address Translation Walk
On x86-64 Long Mode with 48-bit canonical addressing:

```
Virtual Address: [ SignExt (16b) | PML4 (9b) | PDPT (9b) | PD (9b) | PT (9b) | Offset (12b) ]
                         |            |           |          |         |           |
CR3 (PML4 Base) -------->+            |           |          |         |           |
                                      v           |          |         |           |
                           [PML4 Entry (8B)] ---->+          |         |           |
                                                  v          |         |           |
                                       [PDPT Entry (8B)] --->+         |           |
                                                             v         |           |
                                                  [PD Entry (8B)] ---->+           |
                                                                       v           |
                                                            [PT Entry (8B)] ------>+
                                                                                   v
                                                                   Physical Address (PFN + Offset)
```

1. **PML4 Index (Bits 39..47)**: Indexes into the PML4 table to locate the `PML4_ENTRY_64`.
2. **PDPT Index (Bits 30..38)**: Indexes into the Page Directory Pointer Table:
   - If `LargePage (Bit 7) == 1`: Maps a **1GB Huge Page** directly (Physical Base = `PageFrameNumber1GB << 30 | Offset1GB`).
   - If `LargePage (Bit 7) == 0`: Points to the Page Directory.
3. **PD Index (Bits 21..29)**: Indexes into the Page Directory:
   - If `LargePage (Bit 7) == 1`: Maps a **2MB Large Page** directly (Physical Base = `PageFrameNumber2MB << 21 | Offset2MB`).
   - If `LargePage (Bit 7) == 0`: Points to the Page Table.
4. **PT Index (Bits 12..20)**: Indexes into the Page Table to retrieve `PT_ENTRY_64` (4KB physical page frame number).
5. **Page Offset (Bits 0..11)**: Added to `PFN << 12` to compute the exact physical byte.

### 4.3 Hardware TLB Invalidation & Cache Coherency
- **`UnpdInvlpg(virtualAddress)`**: Executes `invlpg byte ptr [rcx]` to invalidate single-page translations in CPU L1/L2 TLBs.
- **`UnpdFlushTlb()`**: Reloads CR3 (`mov rax, cr3; mov cr3, rax`) to flush all non-global TLB entries on the executing core.
- **`UnpdWbinvd()`**: Flushes and writes back all modified internal processor cache lines to system memory.

---

## 5. Polymorphic Memory Engine Architecture (`IMemoryEngine`)

UNPD uses an object-oriented strategy hierarchy for kernel memory allocation:

```cpp
class IMemoryEngine {
public:
    virtual ~IMemoryEngine() = default;
    virtual NTSTATUS Initialize() noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    virtual MemoryMode GetMode() const noexcept = 0;
    virtual const char* GetName() const noexcept = 0;
};
```

1. **`MdlMemoryEngine`**: Implements physical MDL page allocations, user virtual mapping, and atomic double-buffering.
2. **`SlabMemoryEngine`**: High-performance Lookaside lists (`NPAGED_LOOKASIDE_LIST`) for 64B, 256B, 1024B, and 4096B fixed-size blocks.
3. **`PoolMemoryEngine`**: Tracked NonPagedPoolNx allocations via `ExAllocatePool2` with handle tracking.
4. **`DirectNeitherEngine`**: Probed user virtual address validation with Structured Exception Handling (`ProbeForRead` / `ProbeForWrite`).
