# Virtual x86-64 MMU Hardware Sandbox

## Overview

The `VirtualMmu` sandbox (`src/tests/emulator/`) is a 64MB isolated physical RAM simulator that emulates x86-64 hardware paging, page table walks, TLB caching, and page faults in usermode.

This component allows 100% offline verification of memory translation routines and driver protocols in continuous integration (CI) pipelines without requiring a physical virtual machine or WinDbg connection.

---

## Architecture

```
+-----------------------------------------------------------------------+
|                    VirtualMmu Emulator (64MB RAM)                     |
+-----------------------------------------------------------------------+
|  PML4 Table (512 Entries)                                             |
|  ├── PDPT Tables (1GB Pages / Standard PDPT)                          |
|  │   └── Page Directories (2MB Pages / Standard PD)                   |
|  │       └── Page Tables (4KB Standard PTEs)                          |
|  └── 64-Entry LRU Translation Lookaside Buffer (TLB) Cache            |
+-----------------------------------------------------------------------+
|  Page Fault (#PF) Generator:                                          |
|  ├── Not Present Fault (P = 0)                                        |
|  ├── Write-Protect Fault (W/R = 0, Write Attempt)                     |
|  ├── User / Supervisor Fault (U/S Privilege Violation)                |
|  ├── No-Execute Fault (NX Bit Set, Execute Attempt)                   |
|  └── Non-Canonical Address Fault                                      |
+-----------------------------------------------------------------------+
```

---

## Hardware Page Fault Simulation

The emulator evaluates page fault conditions identically to an x86-64 CPU core:

```cpp
enum class PageFaultReason : uint32_t {
    None,
    NotPresent,
    WriteProtect,
    UserSupervisorViolation,
    NoExecuteViolation,
    NonCanonicalAddress
};
```

---

## Verification Metrics

The emulator harness is validated via 12 dedicated GoogleTests (`VirtualMmuTest`):
1. `Basic4KbPageTranslation`
2. `Huge1GbPageTranslation`
3. `Large2MbPageTranslation`
4. `PageFaultNotPresent`
5. `PageFaultWriteProtect`
6. `PageFaultNoExecute`
7. `NonCanonicalAddressFault`
8. `VirtualReadWriteMultiPageChunking`
9. `TlbHitAndInvlpgVerification`
10. `HigherHalfCanonicalTranslation`
11. `UserSupervisorPrivilegeCheck`
12. `MultiPageContiguousBufferChunking`
