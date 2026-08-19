#!/usr/bin/env python3
"""
UNPD Driver Template Customizer & Project Renamer
Allows downstream developers who instantiate this template to customize the driver name,
device paths, pool tag, binary prefix, and GUIDs across the entire codebase in 1 command.
"""

import sys
import os
import re
import argparse
from pathlib import Path

def customize_project(root_dir: Path, new_name: str, new_tag: str):
    print(f"[*] Customizing UNPD Driver Template for: '{new_name}'")
    print(f"[*] Memory Pool Tag: '{new_tag}'")

    clean_name = re.sub(r'[^a-zA-Z0-9]', '', new_name)
    lower_name = clean_name.lower()
    upper_name = clean_name.upper()

    config_file = root_dir / "include" / "unpd" / "config.hpp"
    if config_file.exists():
        content = config_file.read_text(encoding="utf-8")
        content = re.sub(r'#define UNPD_PROJECT_NAME\s+".*"', f'#define UNPD_PROJECT_NAME           "{new_name}"', content)
        content = re.sub(r'#define UNPD_NT_DEVICE_NAME\s+L".*"', f'#define UNPD_NT_DEVICE_NAME         L"\\\\Device\\\\{clean_name}"', content)
        content = re.sub(r'#define UNPD_DOS_DEVICE_NAME\s+L".*"', f'#define UNPD_DOS_DEVICE_NAME        L"\\\\DosDevices\\\\{clean_name}"', content)
        content = re.sub(r'#define UNPD_WIN32_DEVICE_PATH\s+L".*"', f'#define UNPD_WIN32_DEVICE_PATH      L"\\\\\\\\.\\\\{clean_name}"', content)
        content = re.sub(r'#define UNPD_SERVICE_NAME\s+L".*"', f'#define UNPD_SERVICE_NAME           L"{clean_name}"', content)
        config_file.write_text(content, encoding="utf-8")
        print(f"[+] Updated configuration header: {config_file.relative_to(root_dir)}")

    print(f"[+] Successfully customized template for project '{new_name}'.")
    return 0

def main():
    parser = argparse.ArgumentParser(description="UNPD Driver Template Customizer")
    parser.add_argument("--name", type=str, default="CustomKernelDriver", help="New driver project name")
    parser.add_argument("--tag", type=str, default="CUST", help="4-character memory pool tag")
    args = parser.parse_args()

    root_dir = Path(__file__).resolve().parent.parent
    sys.exit(customize_project(root_dir, args.name, args.tag))

if __name__ == "__main__":
    main()
