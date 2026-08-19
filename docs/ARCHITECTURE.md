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
 |     - SSE4.2 CRC32 / Atomic BTS    |
 |     - DR0..DR7 / IDT / GDT / CR8   |
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

## 3. MDL Zero-Copy Shared Memory Lifecycle

```
[ Usermode Process ]                        [ UNPD Driver ]                      [ Windows NT Kernel ]
        |                                          |                                       |
        |--- IOCTL_UNPD_MAP_SHARED_MEMORY -------->|                                       |
        |    (PageCount = 16, Class = 0)           |--- MmAllocatePagesForMdlEx ---------->|
        |                                          |    (Low=0, High=MAX, NonPagedPoolNx)  |
        |                                          |<-- Returns MDL (PFN Array) -----------|
        |                                          |                                       |
        |                                          |--- MmMapLockedPagesSpecifyCache ----->|
        |                                          |    (UserMode, MmCached, NoExecute)    |
        |                                          |<-- Mapped User Virtual Address -------|
        |<-- Returns SessionHandle & UserVA -------|                                       |
        |                                          |                                       |
        |=== Lockless Buffer Exchange (Atomic) ====|                                       |
        |--- Write telemetry into active buffer ---|                                       |
        |--- IOCTL_UNPD_SWAP_BUFFERS ------------->|--- Atomic XCHG + MFENCE ------------->|
        |                                          |--- Invalidate Standby Page (INVLPG) ->|
        |                                          |<-- Standby Buffer Now Active ---------|
        |                                          |                                       |
        |=== Teardown / Process Detach ============|                                       |
        |--- IOCTL_UNPD_UNMAP_SHARED_MEMORY ------>|                                       |
        |                                          |--- KeStackAttachProcess (Process) --->|
        |                                          |--- MmUnmapLockedPages (UserVA) ------>|
        |                                          |--- KeUnstackDetachProcess ----------->|
        |                                          |--- MmFreePagesFromMdl (MDL) --------->|
        |                                          |--- IoFreeMdl (MDL) ------------------>|
        |<-- Status: STATUS_SUCCESS ---------------|                                       |
```

---

## 4. Hardware MMU & 4-Level x86-64 Paging

### 4.1 64-Bit CR3 Register Layout
CR3 register holds the physical address of the Page Map Level 4 (PML4) base table:
- **Bits [51..12]**: PML4 Base Physical Address (4KB aligned).
- **Bits [11..0]**: PCID (Process-Context Identifier).
- **Bit 3 (PWT)**: Page-level Write-Through.
- **Bit 4 (PCD)**: Page-level Cache Disable.

### 4.2 4-Level Linear Translation Walk
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

---

## 6. x86-64 Low-Level Assembly Subsystem & Descriptor Tables

UNPD provides complete architectural access to x86-64 control, debug, descriptor, and performance structures:

### 6.1 Descriptor Table Registers & Segments
- **`DESCRIPTOR_TABLE_REGISTER_64`**: 10-byte structure (`Limit: uint16_t`, `BaseAddress: uint64_t`) for `sgdt` and `sidt`.
- **`IDT_ENTRY_64`**: 16-byte x86-64 Interrupt and Trap Gate descriptor with 64-bit ISR target packing (`SetOffset` / `GetOffset`).
- **`GDT_ENTRY_64`**: 8-byte segment descriptor with 64-bit LongMode and 4KB Granularity bitfields.
- **`TSS64`**: 104-byte Task State Segment defining Ring-0..2 RSP and IST1..IST7 interrupt stacks.
- **Segment Selectors**: `UnpdGetCs`, `UnpdGetDs`, `UnpdGetEs`, `UnpdGetSs`, `UnpdGetFs`, `UnpdGetGs`, `UnpdGetTr`, `UnpdGetLdtr`.

### 6.2 Hardware Debug Registers (DR0..DR7) & CR8 / XCR0
- **`DR7_REGISTER_64`**: Complete bitfield layout for hardware breakpoint conditions (`RW0..RW3`: Execution, Write, I/O, Read/Write) and watchpoint lengths (`LEN0..LEN3`: 1B, 2B, 8B, 4B).
- **`DR0..DR3, DR6`**: Breakpoint linear addresses and debug status register primitives (`UnpdReadDr*` / `UnpdWriteDr*`).
- **`CR8 (TPR)`**: Task Priority Register access (`UnpdReadCr8` / `UnpdWriteCr8`) for direct hardware IRQL management.
- **`XCR0`**: Extended Control Register access (`xgetbv` / `xsetbv`) for AVX, AVX-512, and XSAVE state management.

### 6.3 Hardware-Accelerated SSE4.2 CRC32 & Lockless Bit Manipulation
- **`UnpdComputeCrc32_Buffer`**: High-throughput vectorized QWORD assembly loop utilizing `crc32 rax, rdx` for instantaneous zero-overhead packet and memory integrity validation.
- **`UnpdAtomicBitSet` / `UnpdAtomicBitReset` / `UnpdAtomicBitTest`**: Hardware-locked bitwise primitives (`lock bts`, `lock btr`, `bt`) for lockless bitmap allocation tracking.
