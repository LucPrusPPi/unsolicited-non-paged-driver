# UNPD IOCTL Protocol Specification

## 1. Constants & Identifiers

- **Device Type**: `0x8000` (Third-party custom device range)
- **NT Device Name**: `\Device\UnsolicitedNonPagedDriver`
- **Dos Symlink**: `\DosDevices\UnsolicitedNonPagedDriver`
- **Win32 Usermode Path**: `\\.\UnsolicitedNonPagedDriver`
- **Magic Request Marker**: `0x554E5044` (`'UNPD'`)
- **Magic Response Marker**: `0x44504E55` (`'DPNU'`)
- **Pool Tag**: `'DPNU'` (0x554E5044)

---

## 2. Complete IOCTL Opcode Index

| Function Name | Code | Method | Access Mask | Purpose |
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

## 3. Packet Structures & Detailed Layouts

### IOCTL_UNPD_PING (0x800)
- **Input**: `UNPD_PING_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint32_t Sequence` (Incremental sequence ID)
  - `uint64_t Timestamp` (Caller nanosecond timestamp)
- **Output**: `UNPD_PING_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Sequence` (`Request.Sequence + 1`)
  - `uint64_t KernelTimestamp` (High-precision system time via `KeQuerySystemTimePrecise`)
  - `uint32_t DriverVersionMajor` (1)
  - `uint32_t DriverVersionMinor` (0)

---

### IOCTL_UNPD_ALLOCATE_NONPAGED (0x801)
- **Input**: `UNPD_ALLOC_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint32_t Flags` (Reserved)
  - `uint64_t ByteCount` (Requested allocation size)
- **Output**: `UNPD_ALLOC_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)
  - `uint64_t AllocatedHandle` (Unique 64-bit opaque handle)
  - `uint64_t AllocatedSize` (Actual size committed in kernel pool)

---

### IOCTL_UNPD_FREE_NONPAGED (0x802)
- **Input**: `UNPD_FREE_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint64_t AllocatedHandle` (Handle from previous allocation)
- **Output**: `UNPD_FREE_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or `UNPD_STATUS_NOT_FOUND`)
  - `uint64_t FreedByteCount` (Total bytes returned to system pool)

---

### IOCTL_UNPD_QUERY_STATS (0x803)
- **Output**: `UNPD_STATS_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t ActiveAllocations` (Number of active pool allocations)
  - `uint64_t TotalBytesAllocated` (Cumulative bytes allocated)
  - `uint64_t TotalBytesFreed` (Cumulative bytes freed)
  - `uint64_t TotalIoctlProcessed` (Total requests handled)
  - `uint64_t SpinLockContentionCount` (Lock contention events)
  - `uint64_t TotalSwapsProcessed` (Total atomic double-buffer swaps)
  - `uint32_t ActiveSharedMappings` (Count of active shared memory sessions)

---

### IOCTL_UNPD_MAP_SHARED_MEMORY (0x806)
- **Input**: `UNPD_MAP_SHARED_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint32_t PageCount` (Number of 4KB pages to allocate, 1..256)
- **Output**: `UNPD_MAP_SHARED_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)
  - `uint64_t SessionHandle` (Unique session identifier)
  - `uint64_t UserAddress` (User-mode virtual address of mapped memory)
  - `uint64_t TotalBytes` (Total mapped memory in bytes)
  - `uint32_t BufferSize` (Size per double-buffer half)
  - `uint32_t BufferCount` (2 = Double buffering)

---

### IOCTL_UNPD_UNMAP_SHARED_MEMORY (0x807)
- **Input**: `UNPD_UNMAP_SHARED_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint64_t SessionHandle` (Session identifier to release)
- **Output**: `UNPD_UNMAP_SHARED_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)

---

### IOCTL_UNPD_SWAP_BUFFERS (0x808)
- **Input**: `UNPD_SWAP_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint64_t SessionHandle` (Session identifier)
- **Output**: `UNPD_SWAP_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS`)
  - `uint32_t ActiveBufferIndex` (New active buffer: 0 or 1)
  - `uint32_t StandbyBufferIndex` (New standby buffer: 1 or 0)
  - `uint64_t TotalSwaps` (Cumulative swap count)
  - `uint64_t SwapTimestamp` (Precise timestamp of swap completion)

---

### IOCTL_UNPD_SLAB_ALLOC (0x809)
- **Input**: `UNPD_SLAB_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint32_t BlockClass` (`0` = 64B, `1` = 256B, `2` = 1024B, `3` = 4096B)
- **Output**: `UNPD_SLAB_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS`)
  - `uint64_t SlabHandle` (64-bit block pointer/handle)
  - `uint32_t BlockSize` (Block size in bytes)

---

### IOCTL_UNPD_SLAB_FREE (0x80A)
- **Input**: `UNPD_SLAB_REQUEST` (with `SlabHandle` and `BlockSize`)
- **Output**: `UNPD_FREE_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS`)

---

### IOCTL_UNPD_READ_PROCESS_CR3 (0x80B)
- **Input**: `UNPD_CR3_MEMORY_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint64_t Cr3` (Directory base register of target process)
  - `uint64_t VirtualAddress` (Target process virtual address)
  - `uint64_t UserBuffer` (Caller buffer to receive memory)
  - `uint64_t Size` (Number of bytes to read, 1..16MB)
- **Output**: `UNPD_CR3_MEMORY_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)
  - `uint64_t BytesTransferred` (Actual number of bytes copied)

---

### IOCTL_UNPD_WRITE_PROCESS_CR3 (0x80C)
- **Input**: `UNPD_CR3_MEMORY_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint64_t Cr3` (Directory base register of target process)
  - `uint64_t VirtualAddress` (Target process virtual address)
  - `uint64_t UserBuffer` (Caller buffer containing source bytes)
  - `uint64_t Size` (Number of bytes to write, 1..16MB)
- **Output**: `UNPD_CR3_MEMORY_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)
  - `uint64_t BytesTransferred` (Actual number of bytes written)

---

### IOCTL_UNPD_QUEUE_KAPC (0x80D)
- **Input**: `UNPD_APC_QUEUE_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint32_t TargetThreadId` (Target thread ID)
  - `uint64_t UserRoutine` (User-mode function pointer to execute)
  - `uint64_t UserContext` (Context pointer passed to routine)
- **Output**: `UNPD_APC_QUEUE_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)
  - `uint64_t Reserved` (Alignment padding)

---

### IOCTL_UNPD_CLEAN_PIDDB (0x80E)
- **Input**: `UNPD_STEALTH_PIDDB_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint32_t TimeDateStamp` (Driver PE timestamp)
  - `wchar_t DriverName[64]` (Driver file basename, null-terminated)
- **Output**: `UNPD_STEALTH_PIDDB_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)
  - `uint32_t RemovedCount` (Number of matching records unlinked)

---

### IOCTL_UNPD_CLEAN_UNLOADED (0x80F)
- **Input**: `UNPD_STEALTH_UNLOADED_REQUEST`
  - `uint32_t Magic` (`0x554E5044`)
  - `uint64_t BigPoolAddress` (Optional driver base address in pool)
  - `wchar_t DriverName[64]` (Driver file basename, null-terminated)
- **Output**: `UNPD_STEALTH_UNLOADED_RESPONSE`
  - `uint32_t Magic` (`0x44504E55`)
  - `uint32_t Status` (`UNPD_STATUS_SUCCESS` or error)
  - `uint32_t CleanedCount` (Number of traces compacted)

