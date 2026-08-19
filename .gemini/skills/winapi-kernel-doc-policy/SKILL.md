---
name: winapi-kernel-doc-policy
description: >-
  Enforces comprehensive, exhaustive documentation standards for every Windows NT Kernel API
  and Win32 API function used in C++ and C codebases. Requires explicit IRQL level annotations,
  concurrency constraints, memory pool tags, SEH probe requirements, and NTSTATUS/HRESULT return contracts.
---

# WinAPI & Windows NT Kernel Documentation Policy

## Overview
This skill defines the mandatory documentation protocol for all Windows Kernel-Mode (`_KERNEL_MODE`) and User-Mode Win32 codebases. In low-level systems programming, implicit assumptions lead to BugChecks (`PAGE_FAULT_IN_NONPAGED_AREA`, `IRQL_NOT_LESS_OR_EQUAL`, `DRIVER_VERIFIER_DETECTED_VIOLATION`) and subtle security vulnerabilities.

Every single call to an NT kernel routine or Win32 API must be rigorously documented in source code according to the standards below.

---

## 1. Mandatory Function Header Documentation Standard

For every kernel driver function and IOCTL handler, the function header **MUST** include:

```cpp
/**
 * @brief [Concise summary of what the function accomplishes]
 *
 * @details
 * Detailed operational flow, memory allocation strategies, locks acquired,
 * and hardware / page table interactions.
 *
 * @param[in,out] devExt   Pointer to the device extension containing shared state.
 * @param[in]     irp      Pointer to the I/O Request Packet (IRP).
 * @param[in]     irpSp    Current I/O stack location with IOCTL parameters.
 * @param[out]    info     Bytes transferred or output buffer length written.
 *
 * @irql_requirement
 *   - Minimum IRQL: PASSIVE_LEVEL (0)
 *   - Maximum IRQL: PASSIVE_LEVEL (0) [e.g. Due to MmMapLockedPagesSpecifyCache or ZwMapViewOfSection]
 *
 * @concurrency_model
 *   - Synchronization: Acquires devExt->StateLock (KSPIN_LOCK) at DISPATCH_LEVEL.
 *   - Deadlock Prevention: StateLock must NOT be held while calling paged pool routines or SEH probes.
 *
 * @memory_safety
 *   - Allocation Type: NonPagedPoolNx (Pool tag: 'UNPD')
 *   - Pageability: Function must be in non-paged code section (.text) or PAGED_CODE() if paged.
 *   - User Pointer Safety: Must call ProbeForRead/ProbeForWrite inside __try/__except block.
 *
 * @return NTSTATUS
 *   - STATUS_SUCCESS: Operation completed successfully.
 *   - STATUS_INVALID_PARAMETER: Input payload or sizes failed validation.
 *   - STATUS_INSUFFICIENT_RESOURCES: Pool allocation or MDL mapping failed.
 *   - STATUS_BUFFER_TOO_SMALL: Output buffer is smaller than the required response struct.
 */
```

---

## 2. API-Level Micro-Documentation (Every WinAPI / NT Routine)

Whenever invoking an NT DDK or Win32 function, inline comments must specify:
1. **Purpose**: Why this specific API was chosen over alternatives (e.g. `ExAllocatePool2` vs `ExAllocatePoolWithTag`).
2. **IRQL Safety**: Why it is safe to call at the current IRQL.
3. **Failure Semantics**: How failures are recovered or rolled back without leaking handles/MDLs.

### Examples:

#### A. Physical Page Allocation & MDL Mapping
```cpp
// 1. Allocate physical pages for shared user-mode memory window.
// - API: MmAllocatePagesForMdlEx
// - IRQL Requirement: PASSIVE_LEVEL <= IRQL <= APC_LEVEL
// - Flags: MM_ALLOCATE_PREFER_CONTIGUOUS (reduces TLB miss pressure)
PMDL mdl = MmAllocatePagesForMdlEx(
    lowAddress,
    highAddress,
    skipBytes,
    totalBytes,
    MmCached,
    MM_ALLOCATE_PREFER_CONTIGUOUS
);
if (!mdl) {
    // Return explicit NTSTATUS without bugcheck
    return STATUS_INSUFFICIENT_RESOURCES;
}

// 2. Map locked physical pages into calling process address space.
// - API: MmMapLockedPagesSpecifyCache
// - AccessMode: UserMode (creates PTE in user virtual space 0x000000000000..0x7FFFFFFFFFFF)
// - Protection: NormalPagePriority | MdlMappingNoExecute (W^X DEP compliance)
// - Safety: MUST be wrapped in __try/__except because user address mapping can raise STATUS_ACCESS_VIOLATION.
__try {
    userVa = MmMapLockedPagesSpecifyCache(
        mdl,
        UserMode,
        MmCached,
        NULL,
        FALSE,
        NormalPagePriority | MdlMappingNoExecute
    );
} __except (EXCEPTION_EXECUTE_HANDLER) {
    MmFreePagesFromMdl(mdl);
    IoFreeMdl(mdl);
    return GetExceptionCode();
}
```

#### B. Non-Paged Pool Allocation with Tagging
```cpp
// - API: ExAllocatePool2 (Windows 10 2004+ modern replacement for deprecated ExAllocatePoolWithTag)
// - PoolFlags: POOL_FLAG_NON_PAGED (guaranteed resident in physical RAM at any IRQL <= DISPATCH_LEVEL)
// - PoolTag: 'DPNU' (0x554E5044) for pool leak tracking via PoolMon
PVOID buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'DPNU');
if (!buffer) {
    return STATUS_INSUFFICIENT_RESOURCES;
}
```

#### C. User-Mode Buffer Validation (METHOD_NEITHER)
```cpp
// - API: ProbeForRead / ProbeForWrite
// - IRQL Requirement: PASSIVE_LEVEL (accesses pageable user-mode address space)
// - Alignment: sizeof(ULONG) alignment verification
// - Exception Handling: Required __try/__except block
__try {
    ProbeForRead(userAddress, userLength, sizeof(ULONG));
    ProbeForWrite(userAddress, userLength, sizeof(ULONG));
} __except (EXCEPTION_EXECUTE_HANDLER) {
    return STATUS_DATATYPE_MISALIGNMENT;
}
```

---

## 3. Checklist for Code Review

Before merging or committing any kernel or Win32 file, verify:
- [ ] Every function has an `@irql_requirement` tag documented.
- [ ] Every memory allocation specifies its pool type (`NonPagedPoolNx` / `PagedPool`) and 4-byte Tag.
- [ ] Every user pointer access is enclosed in Structured Exception Handling (`__try` / `__except`).
- [ ] Every synchronization lock (`KSPIN_LOCK`, `FAST_MUTEX`, `ERESOURCE`) has clear ownership and acquisition order documented to prevent inversion deadlocks.
- [ ] Every API return code is explicitly checked and mapped to a valid `NTSTATUS`.
