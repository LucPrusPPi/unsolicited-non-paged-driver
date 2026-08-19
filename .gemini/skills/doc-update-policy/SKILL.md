---
name: doc-update-policy
description: >-
  Enforces continuous, synchronous documentation updates across README, architecture guides,
  IOCTL protocol specifications, and release notes whenever any new kernel feature, MMU structure,
  assembly routine, or test suite is introduced or modified.
---

# Documentation Synchronous Update Policy

## Overview
In high-integrity systems and driver development, out-of-date documentation is considered a critical defect. Whenever code changes are made—such as adding a new IOCTL opcode, introducing a new MMU bitfield structure, expanding the assembly subsystem, or adding test cases—the documentation **MUST** be updated within the same commit or pull request.

---

## 1. Documentation Synchronous Update Protocol

Whenever introducing or altering functionality, perform updates across these 4 artifacts in order:

### A. Root `README.md`
- **Badges**: Update the test count badge (e.g. `tests-29 passed`), CI status, and supported architectures.
- **Repository Tree**: Update file layout in the tree view to reflect new headers, modules, or scripts.
- **Test Matrix Table**: Add newly implemented GoogleTest cases with descriptions and expected outcomes.
- **Subsystem Highlights**: Summarize newly introduced subsystems with concise code snippets.

### B. Architecture Specification (`docs/ARCHITECTURE.md`)
- Document lifecycle, memory pools, concurrency models, and hardware register interactions for the new module.
- Provide clear ASCII or Mermaid sequence diagrams illustrating data flow between Ring-3 usermode and Ring-0 kernel mode.
- Explain IRQL constraints and synchronization invariants.

### C. Protocol Specification (`docs/IOCTL_PROTOCOL.md`)
- If new IOCTLs or structures were added, document:
  - Exact `CTL_CODE` index and numeric opcode.
  - Transfer method (`METHOD_BUFFERED`, `METHOD_IN_DIRECT`, `METHOD_OUT_DIRECT`, `METHOD_NEITHER`).
  - Required access mask (`FILE_READ_DATA`, `FILE_WRITE_DATA`, `FILE_ANY_ACCESS`).
  - Input buffer layout with byte offsets and types.
  - Output buffer layout and status return codes.

### D. Release Notes & Tagging
- Upon completing a milestone with new features, publish a tagged GitHub Release with pre-built signed binaries (`unpd.sys`, `unpd_tests.exe`, `unpd_test_root.cer`).
- Release notes must follow the human-written, non-boilerplate format (summary of changes, test status, SHA256 hashes of attached binaries).

---

## 2. Review Checklist

Before finishing any task that touches code:
- [ ] Are all new headers and source files present in `README.md` repository structure?
- [ ] Is the GoogleTest table in `README.md` matching the exact total count of active test cases?
- [ ] Are all IOCTLs in `common.h` documented in `docs/IOCTL_PROTOCOL.md`?
- [ ] Does `docs/ARCHITECTURE.md` cover all memory modes, MMU structures, and assembly routines?
- [ ] Has a new GitHub Release been published if binaries or major interfaces were updated?
