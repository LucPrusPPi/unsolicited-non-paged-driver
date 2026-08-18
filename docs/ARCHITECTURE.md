# UNPD Kernel Architecture & Design

## Overview

The UNPD (Unsolicited Non-Paged Driver) framework is designed around safety, determinism, and high throughput in Windows NT kernel space. It addresses common failure modes in kernel development, including resource leaks during abnormal process termination, unhandled page faults when accessing user buffers, and race conditions during concurrent dispatch.

---

## Driver Lifecycle and Teardown

```
[ OS Kernel Loader ] 
        |
        v
  DriverEntry()
        |---> IoCreateDevice(FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN)
        |---> Initialize UNPD_DEVICE_EXTENSION (Spinlocks, Allocation List)
        |---> IoCreateSymbolicLink(\\DosDevices\\UnsolicitedNonPagedDriver)
        |---> Register MajorFunction[IRP_MJ_CREATE, IRP_MJ_CLOSE, IRP_MJ_DEVICE_CONTROL]
        |---> Clear DO_DEVICE_INITIALIZING
        v
  [ Active Serving State ] <---> User-Mode IOCTLs
        |
        v (Service Stop / Unload Request)
  DriverUnload()
        |---> IoDeleteSymbolicLink()
        |---> Acquire StateLock (KSPIN_LOCK)
        |---> Iterate and Free all tracked NonPagedPoolNx buffers (ExFreePoolWithTag)
        |---> Release StateLock
        |---> IoDeleteDevice()
        v
[ Driver Terminated Cleanly ]
```

### Initialization Sequence
During `DriverEntry`, the driver allocates a device object with an attached device extension (`UNPD_DEVICE_EXTENSION`) containing its synchronization and tracking state. The symbolic link `\\DosDevices\\UnsolicitedNonPagedDriver` is created in the NT object manager namespace, exposing the interface to user-mode callers via `\\.\UnsolicitedNonPagedDriver`. Dispatch routines for create, close, and device control are assigned to the `DriverObject->MajorFunction` table before clearing the `DO_DEVICE_INITIALIZING` bit.

### Teardown Sequence
During `DriverUnload`, the driver ensures that no lingering kernel allocations or dangling symbolic links remain in memory. The symbolic link is unregistered first to prevent incoming create requests. The device extension spinlock is acquired while iterating over the allocation tracking list, deallocating all registered memory blocks with `ExFreePoolWithTag`. Finally, the device object is deleted with `IoDeleteDevice`.

---

## Memory Subsystem & Allocation Tracking

Dynamic memory allocations target non-executable non-paged pool memory using `ExAllocatePool2`:

```cpp
PVOID buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, UNPD_POOL_TAG);
```

### Subsystem Design
1. Zero Initialization: `ExAllocatePool2` guarantees zeroed memory, mitigating kernel information leak vulnerabilities (CWE-200).
2. Execution Prevention: Non-paged memory is mapped non-executable by default, preventing arbitrary shellcode execution in pool memory.
3. Pool Tagging: Every block is tagged with `'UNPD'` (`0x554E5044`), allowing developers to inspect pool usage via Windows Kernel Debugger:
   ```text
   kd> !poolfind UNPD
   kd> !poolused 2 UNPD
   ```
4. Handle Table: Allocations exposed to usermode are assigned 64-bit handles stored in an intrusive doubly-linked list (`LIST_ENTRY`), protected by `devExt->StateLock`.

---

## Kernel RAII Primitives

The driver implements RAII abstractions in `include/unpd/kernel_raii.hpp` without relying on C++ exceptions or runtime type information:

```cpp
class SpinlockGuard {
public:
    explicit SpinlockGuard(PKSPIN_LOCK lock) noexcept
        : m_lock(lock), m_oldIrql(PASSIVE_LEVEL) {
        KeAcquireSpinLock(m_lock, &m_oldIrql);
    }

    ~SpinlockGuard() noexcept {
        if (m_lock != nullptr) {
            KeReleaseSpinLock(m_lock, m_oldIrql);
        }
    }
};
```

This ensures that spinlocks and fast mutexes are consistently released and IRQL levels are restored upon leaving scope, even during early exit conditions.

---

## I/O Buffer Transfer Mechanisms

UNPD implements reference handlers for all three Windows I/O transfer methods:

### 1. METHOD_BUFFERED
The I/O Manager allocates an intermediate system buffer in kernel space (`Irp->AssociatedIrp.SystemBuffer`). Used for lightweight control packets (`IOCTL_UNPD_PING`, `IOCTL_UNPD_QUERY_STATS`, `IOCTL_UNPD_ALLOCATE_NONPAGED`, `IOCTL_UNPD_FREE_NONPAGED`).

### 2. METHOD_OUT_DIRECT
The I/O Manager creates a Memory Descriptor List (`MDL`) describing the caller pages and locks them in physical memory. The driver maps the MDL safely using `MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority | MdlMappingNoExecute)` for high-throughput zero-copy transfers.

### 3. METHOD_NEITHER
Raw user-mode virtual addresses are provided in `irpSp->Parameters.DeviceIoControl.Type3InputBuffer` and `irp->UserBuffer`. The driver performs alignment checks and boundary probing via `ProbeForRead` and `ProbeForWrite` enclosed in `__try` / `__except` blocks to trap invalid addresses without causing a BugCheck.
