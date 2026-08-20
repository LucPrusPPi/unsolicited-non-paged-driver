# Zero-BSOD Safety & BugCheck Invariants

## Overview

Operating in Windows NT Kernel Mode (Ring-0) leaves zero tolerance for unhandled exceptions or invalid IRQL accesses. UNPD enforces a strict set of safety invariants guaranteeing that no user-space input or internal error can trigger a system crash (BSOD).

---

## Defensive Invariant Matrix

| BugCheck Code | Description | Root Cause Prevented | Defensive Invariant in UNPD |
|---|---|---|---|
| **`0x0A`** | `IRQL_NOT_LESS_OR_EQUAL` | Accessing paged pool memory at `DISPATCH_LEVEL` or calling `MmUnmapLockedPages` under spinlock | All kernel structures and pools are allocated strictly from `NonPagedPoolNx`. Process exit notifications (`PsSetCreateProcessNotifyRoutineEx`) safely nullify `UserVa` under spinlock, avoiding illegal IRQL API calls. |
| **`0x1E`** | `KMODE_EXCEPTION_NOT_HANDLED` | Unhandled C++ exception or access violation in kernel mode | Exception handling in Ring-0 is exception-free via `kstd::expected`. User memory dereferences (including `ResolveVmt`) are isolated in local `__try / __except (EXCEPTION_EXECUTE_HANDLER)`. |
| **`0x3B`** | `SYSTEM_SERVICE_EXCEPTION` | Invalid or unaligned user pointer passed to system service | Mandatory alignment checks and probing via `ProbeForRead` and `ProbeForWrite` prior to dereferencing any user-mode address. |
| **`0x3F`** | `NO_MORE_SYSTEM_PTES` | Leaking System PTEs on SEH exceptions bypassing C++ destructors | `RtlCopyMemory` operations in physical memory mapping are isolated in local SEH blocks inside RAII scope, ensuring `~PhysicalMemoryMapping` always invokes `MmUnmapIoSpace`. |
| **`0x50`** | `PAGE_FAULT_IN_NONPAGED_AREA` | Accessing unmapped physical/virtual address space in scanner or walker | Pattern scanner uses `IoAllocateMdl` + `MmProbeAndLockPages` to physically lock target memory pages in RAM, preventing TOCTOU page faults during SIMD scanning. |
| **`0x76`** | `PROCESS_HAS_LOCKED_PAGES` | Process termination before driver unmaps user-mapped MDL pages | Process termination callback registered via `PsSetCreateProcessNotifyRoutineEx` intercepts dying processes and cleans up mappings before VAD destruction. |
| **`0x7E`** | `SYSTEM_THREAD_EXCEPTION_NOT_HANDLED` | Thread crash before APC execution or rundown | `KernelApcRundown` and `KernelApcCleanup` routines guarantee pool deallocation (`ExFreePoolWithTag`) even if the target thread terminates prematurely. |
| **`0xD1`** | `DRIVER_IRQL_NOT_LESS_OR_EQUAL` | Spinlock held across pageable code boundaries | All spinlock-protected critical sections contain purely non-paged memory operations and atomic primitives. |
| **`0x109`** | `CRITICAL_STRUCTURE_CORRUPTION` | PatchGuard / Kernel Patch Protection (KPP) trigger | Zero static hooks; all trace scrubbing uses native AVL table rebalancing and circular list compaction. |

---

## SEH Probing Pattern

Every user-mode pointer handled by the IOCTL router follows this safe pattern:

```cpp
NTSTATUS SafeUserRead(const void* userBuffer, SIZE_T size, void* kernelBuffer) {
    if (!userBuffer || !kernelBuffer || size == 0 || size > UNPD_MAX_BUFFER_SIZE) {
        return STATUS_INVALID_PARAMETER;
    }

    __try {
        ProbeForRead(reinterpret_cast<PVOID>(const_cast<void*>(userBuffer)), size, 1);
        memcpy(kernelBuffer, userBuffer, size);
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
}
```
