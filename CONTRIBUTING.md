# Contributing to UNPD

We welcome contributions to the Unsolicited Non-Paged Driver (UNPD) project. Please follow these guidelines before submitting code.

## Development Standards

- **Language Standard**: C++20 for both kernel driver and test suite.
- **Kernel Code Rules**:
  - No exceptions (`/EHsc-`) or RTTI (`/GR-`).
  - Use RAII wrappers (`unpd::SpinlockGuard`, `unpd::FastMutexGuard`) instead of manual lock acquisition and `goto cleanup`.
  - Always use `ExAllocatePool2` with `POOL_FLAG_NON_PAGED` and `'UNPD'` tag.
  - Wrap all user-mode memory probing in Structured Exception Handling (`__try` / `__except`).
- **Formatting**: Format all C++ code using `clang-format` before submitting.
- **Compiler Compatibility**: Changes must build cleanly under both MSVC (`cl.exe`) and Clang (`clang-cl.exe`) with `/W4 /WX`.

## Submitting Pull Requests

1. Fork the repository and create your branch from `main`.
2. Ensure tests build and pass via `.\build.ps1 -Compiler Auto -Clean -Sign`.
3. Open a Pull Request with a clear description of the changes and testing performed.
