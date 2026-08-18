# UNPD IOCTL Protocol Specification

## 1. Constants & Identifiers

- Device Type: `0x8000` (User-defined third-party device range)
- Device Path: `\Device\UnsolicitedNonPagedDriver`
- Dos Device Symlink: `\DosDevices\UnsolicitedNonPagedDriver`
- Win32 Usermode Path: `\\.\UnsolicitedNonPagedDriver`
- Magic Request: `0x554E5044` (`'UNPD'`)
- Magic Response: `0x44504E55` (`'DPNU'`)

---

## 2. Control Codes Specification

### IOCTL_UNPD_PING (0x800)
- Transfer Method: `METHOD_BUFFERED`
- Required Access: `FILE_READ_ACCESS | FILE_WRITE_ACCESS`
- Purpose: Verifies round-trip latency and communication integrity between user space and kernel space.
- Input Buffer: `UNPD_PING_REQUEST`
  - `Magic`: `0x554E5044`
  - `Sequence`: 32-bit incremental sequence number
  - `Timestamp`: 64-bit nanosecond timestamp from user mode
- Output Buffer: `UNPD_PING_RESPONSE`
  - `Magic`: `0x44504E55`
  - `Sequence`: `Request.Sequence + 1`
  - `KernelTimestamp`: 64-bit high-precision system tick (`KeQuerySystemTimePrecise`)
  - `DriverVersionMajor` / `DriverVersionMinor`: Driver version numbers (1.0)

---

### IOCTL_UNPD_ALLOCATE_NONPAGED (0x801)
- Transfer Method: `METHOD_BUFFERED`
- Required Access: `FILE_READ_ACCESS | FILE_WRITE_ACCESS`
- Purpose: Allocates a physical non-paged memory block in kernel pool (`NonPagedPoolNx`) and registers it in the device handle table.
- Input Buffer: `UNPD_ALLOC_REQUEST`
  - `Magic`: `0x554E5044`
  - `Flags`: Reserved options
  - `ByteCount`: Requested allocation size in bytes
- Output Buffer: `UNPD_ALLOC_RESPONSE`
  - `Magic`: `0x44504E55`
  - `Status`: `UNPD_STATUS_SUCCESS` or error code
  - `AllocatedHandle`: Unique 64-bit opaque handle referencing the kernel allocation
  - `AllocatedSize`: Actual size allocated in kernel space

---

### IOCTL_UNPD_FREE_NONPAGED (0x802)
- Transfer Method: `METHOD_BUFFERED`
- Required Access: `FILE_READ_ACCESS | FILE_WRITE_ACCESS`
- Purpose: Deallocates a kernel memory block referenced by its 64-bit handle.
- Input Buffer: `UNPD_FREE_REQUEST`
  - `Magic`: `0x554E5044`
  - `AllocatedHandle`: Handle returned from a previous allocation request
- Output Buffer: `UNPD_FREE_RESPONSE`
  - `Magic`: `0x44504E55`
  - `Status`: `UNPD_STATUS_SUCCESS` or `UNPD_STATUS_NOT_FOUND`

---

### IOCTL_UNPD_QUERY_STATS (0x803)
- Transfer Method: `METHOD_BUFFERED`
- Required Access: `FILE_READ_ACCESS`
- Purpose: Returns global runtime metrics and memory allocation statistics.
- Output Buffer: `UNPD_STATS_RESPONSE`
  - `Magic`: `0x44504E55`
  - `ActiveAllocations`: Count of currently tracked memory blocks
  - `TotalBytesAllocated`: Cumulative bytes allocated
  - `TotalBytesFreed`: Cumulative bytes deallocated
  - `TotalIoctlProcessed`: Total IRP requests processed
  - `SpinLockContentionCount`: Lock contention metrics

---

### IOCTL_UNPD_PROCESS_BUFFER_DIRECT (0x804)
- Transfer Method: `METHOD_OUT_DIRECT`
- Required Access: `FILE_READ_ACCESS | FILE_WRITE_ACCESS`
- Purpose: Demonstrates high-performance zero-copy buffer transfers using Memory Descriptor Lists (`MDL`).

---

### IOCTL_UNPD_PROCESS_BUFFER_NEITHER (0x805)
- Transfer Method: `METHOD_NEITHER`
- Required Access: `FILE_READ_ACCESS | FILE_WRITE_ACCESS`
- Purpose: Demonstrates manual user-space buffer probing with `ProbeForRead` and `ProbeForWrite` under Structured Exception Handling (`__try` / `__except`).
