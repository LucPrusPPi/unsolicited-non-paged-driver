#!/usr/bin/env python3
"""
UNPD Driver Framework - Repository Language Breakdown & Statistics Generator
Scans repository code files, computes bytes, lines of code (SLOC), percentages,
and updates docs/LANGUAGES.md automatically.
"""

import os
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
DOCS_DIR = ROOT_DIR / "docs"
TARGET_FILE = DOCS_DIR / "LANGUAGES.md"

# Language classification definitions according to .gitattributes
LANGUAGE_RULES = {
    "C++": {
        "extensions": [".cpp", ".hpp"],
        "color": "🔴",
        "description": "Kernel driver core (unpd.sys), MMU paging engine, client SDK, and GoogleTest harness"
    },
    "C": {
        "patterns": ["common.h"],
        "color": "⚪",
        "description": "Ring-0/Ring-3 shared IOCTL protocol definitions and NT status constants"
    },
    "Assembly": {
        "extensions": [".asm"],
        "color": "🟤",
        "description": "x86-64 MASM low-level control registers, memory barriers, SSE4.2 CRC32, and atomics"
    },
    "Lua": {
        "extensions": [".lua"],
        "color": "🌙",
        "description": "MMU translation simulator, binary packet codec, telemetry profiler, and fuzzer"
    },
    "Python": {
        "extensions": [".py"],
        "color": "🔵",
        "description": "Latency percentile analyzer, Win32 SCM controller, template customizer, and fuzzer"
    },
    "PowerShell": {
        "extensions": [".ps1"],
        "color": "🔷",
        "description": "Windows automated build runners, SCM service managers, SignTool, and VM setup"
    },
    "Shell": {
        "extensions": [".sh"],
        "color": "🟢",
        "description": "POSIX / Git Bash / WSL build scripts, package release, and CI check wrappers"
    },
    "CMake": {
        "extensions": [".cmake"],
        "filenames": ["CMakeLists.txt"],
        "color": "🔴",
        "description": "Cross-compiler build orchestration, vcpkg exports, and target configurations"
    }
}

IGNORE_DIRS = {".git", "build", "ports", ".gemini", ".claude", "__pycache__", ".vscode", ".idea"}

def scan_repository():
    stats = {lang: {"files": 0, "lines": 0, "bytes": 0} for lang in LANGUAGE_RULES}

    for root, dirs, files in os.walk(ROOT_DIR):
        # Filter out ignored directories
        dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]

        for file in files:
            file_path = Path(root) / file
            rel_path = file_path.relative_to(ROOT_DIR)

            # Skip root build artifacts or temporary files
            if any(part in IGNORE_DIRS for part in rel_path.parts):
                continue

            matched_lang = None

            # Special case for C (common.h)
            if file == "common.h":
                matched_lang = "C"
            else:
                ext = file_path.suffix.lower()
                for lang, rules in LANGUAGE_RULES.items():
                    if lang == "C":
                        continue
                    if "filenames" in rules and file in rules["filenames"]:
                        matched_lang = lang
                        break
                    if "extensions" in rules and ext in rules["extensions"]:
                        matched_lang = lang
                        break

            if matched_lang:
                try:
                    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                        lines = len(f.readlines())
                    size = file_path.stat().st_size
                    stats[matched_lang]["files"] += 1
                    stats[matched_lang]["lines"] += lines
                    stats[matched_lang]["bytes"] += size
                except Exception:
                    pass

    return stats

def generate_progress_bar(percentage, length=18):
    filled = int(round((percentage / 100) * length))
    return "█" * filled + "░" * (length - filled)

def generate_markdown(stats):
    total_bytes = sum(s["bytes"] for s in stats.values())
    total_lines = sum(s["lines"] for s in stats.values())
    total_files = sum(s["files"] for s in stats.values())

    # Sort languages by bytes descending
    sorted_langs = sorted(stats.items(), key=lambda x: x[1]["bytes"], reverse=True)

    lines = []
    lines.append("# UNPD Repository Language Metrics & Breakdown")
    lines.append("")
    lines.append("Automated codebase composition and language distribution metrics for the **Unsolicited Non-Paged Driver (UNPD)** repository.")
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append("## Language Distribution Summary")
    lines.append("")
    lines.append("| Language | Distribution Bar | Share (%) | Files | Code Lines (SLOC) | Size (Bytes) |")
    lines.append("|:---|:---|:---:|:---:|:---:|:---:|")

    for lang, data in sorted_langs:
        pct = (data["bytes"] / total_bytes * 100) if total_bytes > 0 else 0
        bar = generate_progress_bar(pct)
        color = LANGUAGE_RULES[lang]["color"]
        lines.append(f"| {color} **{lang}** | `{bar}` | **{pct:5.2f}%** | {data['files']} | {data['lines']:,} | {data['bytes']:,} B |")

    lines.append(f"| **Total** | | **100.00%** | **{total_files}** | **{total_lines:,}** | **{total_bytes:,} B** |")
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append("## Subsystem Roles by Language")
    lines.append("")
    lines.append("| Language | Primary Role & Responsibility in UNPD |")
    lines.append("|:---|:---|")
    for lang, _ in sorted_langs:
        desc = LANGUAGE_RULES[lang]["description"]
        color = LANGUAGE_RULES[lang]["color"]
        lines.append(f"| {color} **{lang}** | {desc} |")

    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append("> [!NOTE]")
    lines.append("> This report is verified and maintained by `scripts/python/update_language_stats.py`.")
    lines.append("")

    return "\n".join(lines)

def main():
    check_mode = "--check" in sys.argv
    stats = scan_repository()
    content = generate_markdown(stats)

    DOCS_DIR.mkdir(parents=True, exist_ok=True)

    if check_mode:
        if TARGET_FILE.exists():
            with open(TARGET_FILE, "r", encoding="utf-8") as f:
                existing = f.read()
            if existing.strip() == content.strip():
                print("[+] docs/LANGUAGES.md is up to date.")
                return 0
            else:
                print("[!] docs/LANGUAGES.md is out of date. Run without --check to update.")
                return 1
        else:
            print("[!] docs/LANGUAGES.md does not exist.")
            return 1
    else:
        with open(TARGET_FILE, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"[+] Successfully generated and updated {TARGET_FILE}")
        return 0

if __name__ == "__main__":
    sys.exit(main())
