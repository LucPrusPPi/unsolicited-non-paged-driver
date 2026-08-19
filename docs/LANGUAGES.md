# UNPD Repository Language Metrics & Breakdown

Automated codebase composition and language distribution metrics for the **Unsolicited Non-Paged Driver (UNPD)** repository.

---

## Language Distribution Summary

| Language | Distribution Bar | Share (%) | Files | Code Lines (SLOC) | Size (Bytes) |
|:---|:---|:---:|:---:|:---:|:---:|
| 🔴 **C++** | `██████████░░░░░░░░` | **57.22%** | 33 | 4,765 | 146,417 B |
| 🌙 **Lua** | `██░░░░░░░░░░░░░░░░` | **10.63%** | 7 | 797 | 27,186 B |
| 🔵 **Python** | `██░░░░░░░░░░░░░░░░` | **10.48%** | 7 | 743 | 26,815 B |
| 🟤 **Assembly** | `█░░░░░░░░░░░░░░░░░` | ** 5.29%** | 1 | 515 | 13,539 B |
| 🔷 **PowerShell** | `█░░░░░░░░░░░░░░░░░` | ** 5.15%** | 5 | 324 | 13,181 B |
| 🟢 **Shell** | `█░░░░░░░░░░░░░░░░░` | ** 4.84%** | 7 | 345 | 12,395 B |
| 🔴 **CMake** | `█░░░░░░░░░░░░░░░░░` | ** 3.61%** | 1 | 272 | 9,248 B |
| ⚪ **C** | `░░░░░░░░░░░░░░░░░░` | ** 2.77%** | 1 | 217 | 7,085 B |
| **Total** | | **100.00%** | **62** | **7,978** | **255,866 B** |

---

## Subsystem Roles by Language

| Language | Primary Role & Responsibility in UNPD |
|:---|:---|
| 🔴 **C++** | Kernel driver core (unpd.sys), MMU paging engine, client SDK, and GoogleTest harness |
| 🌙 **Lua** | MMU translation simulator, binary packet codec, telemetry profiler, and fuzzer |
| 🔵 **Python** | Latency percentile analyzer, Win32 SCM controller, template customizer, and fuzzer |
| 🟤 **Assembly** | x86-64 MASM low-level control registers, memory barriers, SSE4.2 CRC32, and atomics |
| 🔷 **PowerShell** | Windows automated build runners, SCM service managers, SignTool, and VM setup |
| 🟢 **Shell** | POSIX / Git Bash / WSL build scripts, package release, and CI check wrappers |
| 🔴 **CMake** | Cross-compiler build orchestration, vcpkg exports, and target configurations |
| ⚪ **C** | Ring-0/Ring-3 shared IOCTL protocol definitions and NT status constants |

---

> [!NOTE]
> This report is verified and maintained by `scripts/python/update_language_stats.py`.
