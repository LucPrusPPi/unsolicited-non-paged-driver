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
| **`0x7F`** | `UNEXPECTED_KERNEL_MODE_TRAP` (Stack Overflow) | Allocating heavy SIMD/AVX context buffers on limited 24KB kernel stack | Kernel memory copies utilize lightweight hardware ERMS/FSRM (`rep movsb`/`rep stosb`), eliminating large `XSTATE_SAVE` stack allocations. |
| **`0x109`** | `CRITICAL_STRUCTURE_CORRUPTION` | PatchGuard / Kernel Patch Protection (KPP) validation | Unexported table scrubbers (`PiDDBCacheTable`, `MmUnloadedDrivers`) perform in-place AVL rebalancing and list compaction. *Note: In production environments with active PatchGuard, direct table modifications will trigger periodic KPP DPC audits; these features are intended for test-signing, virtualized analysis, or bootkit environments.* |
| **`0x124`** | `WHEA_UNCORRECTABLE_ERROR` | Machine Check Exception (MCE) from PAT/MTRR cache aliasing (UC vs WB) on physical RAM | Physical memory reader uses `MmCopyMemory` with `MM_COPY_MEMORY_PHYSICAL` instead of `MmMapIoSpace(..., MmNonCached)`. |

---

## Hardened Security Invariants & Zero-Day Defense Matrix

| Vulnerability / Zero-Day Class | Vector & Attack Surface | Root Cause | Implemented Defensive Invariant |
|---|---|---|---|
| **VirtualFree DoS / PFN Corruption (`0x4E`/`0x76`)** | User-mode application calling `VirtualFree` on mapped MDL pages before IOCTL Unmap | Memory Manager destroys VAD, causing BugCheck on subsequent `MmUnmapLockedPages`. | Immediate `MmSecureVirtualMemory(userVa, size, PAGE_READWRITE)` on mapping; user-mode freeing/protection changes are rejected by kernel. Safe `MmUnsecureVirtualMemory` on teardown. |
| **Access Mode Spoofing & KASLR Leak** | Passing kernel address (`0xFFFF...`) to SIMD scanner with dynamic `accessMode` calculation | `accessMode` switched to `KernelMode`, bypassing security probe and allowing user mode to scan/leak kernel pointers. | Enforced `irp->RequestorMode` with strict check: user-mode requests attempting to scan addresses `>= 0x7FFFFFFFFFFF` immediately return `STATUS_ACCESS_DENIED`. |
| **Unprobed Kernel Read in VMT Resolver** | User mode passing kernel address in `ModuleBase` to `UnpdHandleResolveVmt` | Missing `ProbeForRead` allowed reading kernel VTables across address boundaries without protection verification. | Mandatory `ProbeForRead(ModuleBase, ModuleSize, 1)` within `__try / __except` for all user-mode requests. |
| **Integer Truncation in Memory Lock** | Passing `BufferSize = 0x100000000` (4GB + 1B) cast to 32-bit `ULONG` in `IoAllocateMdl` | Truncation resulted in 1 locked page while scanner iterated through unlocked physical RAM (TOCTOU). | Strict `kMaxScanBufferSize = 16MB` upper-bound validation on all scan and CR3 read/write requests. |
| **Down-level Boot/Load Failure (`ExAllocatePool2`)** | Missing `ExAllocatePool2` export on Windows 10 < 2004 (1909, 1809, LTSB 2016) | Direct calls to `ExAllocatePool2` resulted in `STATUS_ENTRYPOINT_NOT_FOUND` / BugCheck `0x7E`. | Universal `UnpdAllocatePool` / `UnpdFreePool` wrappers dynamically selecting `ExAllocatePool2` or `ExAllocatePoolWithTag(NonPagedPoolNx)` based on OS build. |
| **Uninitialized Kernel Stack Leak** | Missing assignment to `FreedByteCount` in `UnpdHandleFree` / `UnpdHandleSlabFree` | 8 bytes of uninitialized kernel stack data copied back to user mode buffer. | Explicit assignment of `FreedByteCount = freeSize` and total zero-initialization of response structures. |
| **METHOD_IN_DIRECT Buffer Role Inversion** | Attempting to read input request magic from `MmGetSystemAddressForMdlSafe(irp->MdlAddress)` | In `METHOD_IN_DIRECT`, input is in `SystemBuffer`, output is in `MdlAddress`. | Input read strictly from `irp->AssociatedIrp.SystemBuffer`, output checksum written to `irp->MdlAddress`. |

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
