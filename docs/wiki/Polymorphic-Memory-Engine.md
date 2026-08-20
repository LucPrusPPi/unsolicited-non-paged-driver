# Polymorphic Memory Engine Architecture (`IMemoryEngine`)

## Abstract Interface Contract

The `IMemoryEngine` abstract base class enforces a polymorphic strategy pattern across all kernel memory management subsystems:

```cpp
class IMemoryEngine {
public:
    virtual ~IMemoryEngine() = default;
    virtual NTSTATUS Initialize() noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    [[nodiscard]] virtual MemoryMode GetMode() const noexcept = 0;
    [[nodiscard]] virtual const char* GetName() const noexcept = 0;
};
```

---

## Engine Implementations

### 1. `MdlMemoryEngine` (Zero-Copy Physical MDL Mapping)
- **Allocation Mechanism**: Allocates non-contiguous physical pages via `MmAllocatePagesForMdlEx`.
- **User Mapping**: Maps allocated MDL pages into user virtual address space using `MmMapLockedPagesSpecifyCache` with `MdlMappingNoExecute` (enforcing Hardware DEP/NX protection).
- **Process Attachment Guard**: During teardown, if the calling process differs from the owning process, `ProcessAttachmentGuard` executes `KeStackAttachProcess` to safely invoke `MmUnmapLockedPages`, then calls `MmFreePagesFromMdl`, `IoFreeMdl`, and `ObDereferenceObject`.

### 2. `SlabMemoryEngine` (Lookaside List Cache Pool)
- **Allocation Mechanism**: $O(1)$ allocation/deallocation via `NPAGED_LOOKASIDE_LIST`.
- **Slab Classes**:
  - Class 0: 64-byte blocks (Tag: `'1LSU'`)
  - Class 1: 256-byte blocks (Tag: `'2LSU'`)
  - Class 2: 1024-byte blocks (Tag: `'3LSU'`)
  - Class 3: 4096-byte blocks (Tag: `'4LSU'`)
- Eliminates kernel pool fragmentation for high-frequency small allocations.

### 3. `PoolMemoryEngine` (Tracked NonPaged Pools)
- **Allocation Mechanism**: Allocates non-pageable memory via `ExAllocatePool2(POOL_FLAG_NON_PAGED, size, tag)`.
- **Handle Indexing**: Maintains an internal table of 64-bit handles mapping to pool headers.
- **Spinlock Protection**: All session additions and removals are synchronized via `KeAcquireSpinLock`.

### 4. `DirectNeitherEngine` (Probed Neither I/O)
- **Handling**: Used for `METHOD_NEITHER` and `METHOD_IN_DIRECT` IOCTL descriptors.
- **Probing**: Executes `ProbeForRead` and `ProbeForWrite` with explicit alignment checks inside `__try / __except (EXCEPTION_EXECUTE_HANDLER)`.
- Prevents malicious or unmapped user-space pointers from inducing kernel panics.
