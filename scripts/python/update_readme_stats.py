#!/usr/bin/env python3
"""
Automatically scans repository languages and updates the Languages table in README.md.
"""

import os
import re
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
README_FILE = ROOT_DIR / "README.md"

LANGUAGE_RULES = {
    "C++": {"extensions": [".cpp", ".hpp"]},
    "Lua": {"extensions": [".lua"]},
    "Python": {"extensions": [".py"]},
    "Assembly": {"extensions": [".asm"]},
    "PowerShell": {"extensions": [".ps1"]},
    "Shell": {"extensions": [".sh"]},
    "CMake": {"extensions": [".cmake"], "filenames": ["CMakeLists.txt"]},
    "C": {"filenames": ["common.h"]}
}

IGNORE_DIRS = {".git", "build", "ports", ".gemini", ".claude", "__pycache__", ".vscode", ".idea"}

def scan_repository():
    stats = {lang: {"files": 0, "lines": 0, "bytes": 0} for lang in LANGUAGE_RULES}

    for root, dirs, files in os.walk(ROOT_DIR):
        dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]

        for file in files:
            file_path = Path(root) / file
            rel_path = file_path.relative_to(ROOT_DIR)

            if any(part in IGNORE_DIRS for part in rel_path.parts):
                continue

            matched_lang = None
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
                        content = f.read().replace("\r\n", "\n")
                    stats[matched_lang]["files"] += 1
                    stats[matched_lang]["lines"] += len(content.splitlines())
                    stats[matched_lang]["bytes"] += len(content.encode("utf-8"))
                except Exception:
                    pass

    return stats

def update_readme():
    if not README_FILE.exists():
        print(f"[!] README.md not found at {README_FILE}")
        return 1

    stats = scan_repository()
    total_bytes = sum(s["bytes"] for s in stats.values())

    sorted_langs = sorted(stats.items(), key=lambda x: x[1]["bytes"], reverse=True)

    table_lines = [
        "<!-- LANGUAGES_START -->",
        "| Language | Share | Files | Code Lines |",
        "|---|---|---|---|"
    ]

    for lang, data in sorted_langs:
        pct = (data["bytes"] / total_bytes * 100) if total_bytes > 0 else 0
        table_lines.append(f"| {lang} | {pct:4.1f}% | {data['files']} | {data['lines']:,} |")

    table_lines.append("<!-- LANGUAGES_END -->")
    new_table_str = "\n".join(table_lines)

    with open(README_FILE, "r", encoding="utf-8") as f:
        readme_content = f.read()

    pattern = re.compile(r"<!-- LANGUAGES_START -->.*?<!-- LANGUAGES_END -->", re.DOTALL)
    if not pattern.search(readme_content):
        print("[!] Markers <!-- LANGUAGES_START --> ... <!-- LANGUAGES_END --> not found in README.md")
        return 1

    updated_content = pattern.sub(new_table_str, readme_content)

    with open(README_FILE, "w", encoding="utf-8") as f:
        f.write(updated_content)

    print("[+] Successfully updated Languages table in README.md")
    return 0

if __name__ == "__main__":
    sys.exit(update_readme())
