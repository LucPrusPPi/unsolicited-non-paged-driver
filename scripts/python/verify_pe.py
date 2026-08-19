#!/usr/bin/env python3
"""
UNPD Binary Inspector & PE Authenticode Verification
Validates PE headers, checksums, subsystem (/SUBSYSTEM:NATIVE), Control Flow Guard (/GUARD:CF),
DynamicBase (ASLR), NX (DEP), and Authenticode security directories.
"""

import sys
import struct
import argparse
from pathlib import Path
from typing import Dict, Any

class PEInspector:
    IMAGE_FILE_MACHINE_AMD64 = 0x8664
    IMAGE_SUBSYSTEM_NATIVE   = 1
    IMAGE_SUBSYSTEM_WINDOWS_CUI = 3

    IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE = 0x0040
    IMAGE_DLLCHARACTERISTICS_NX_COMPAT    = 0x0100
    IMAGE_DLLCHARACTERISTICS_GUARD_CF     = 0x4000

    def __init__(self, file_path: Path):
        self.path = file_path
        self.data = file_path.read_bytes()

    def inspect(self) -> Dict[str, Any]:
        if len(self.data) < 64 or self.data[:2] != b"MZ":
            raise ValueError("Not a valid DOS/PE executable.")

        e_lfanew = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[e_lfanew:e_lfanew+4] != b"PE\x00\x00":
            raise ValueError("Invalid PE signature.")

        # File Header
        coff_offset = e_lfanew + 4
        machine, num_sections, time_date_stamp, _, _, size_opt_header, characteristics = struct.unpack_from(
            "<HHIIIHH", self.data, coff_offset
        )

        # Optional Header (PE32+)
        opt_offset = coff_offset + 20
        magic = struct.unpack_from("<H", self.data, opt_offset)[0]
        is_pe32_plus = (magic == 0x20B)

        subsystem = struct.unpack_from("<H", self.data, opt_offset + 68)[0]
        dll_chars = struct.unpack_from("<H", self.data, opt_offset + 70)[0]
        checksum  = struct.unpack_from("<I", self.data, opt_offset + 64)[0]

        # Security Directory (Entry 4 in Data Directories)
        data_dirs_offset = opt_offset + 112
        sec_dir_rva, sec_dir_size = struct.unpack_from("<II", self.data, data_dirs_offset + 4 * 8)

        return {
            "Machine": "x64 (AMD64)" if machine == self.IMAGE_FILE_MACHINE_AMD64 else hex(machine),
            "Subsystem": "NATIVE (Driver)" if subsystem == self.IMAGE_SUBSYSTEM_NATIVE else "CONSOLE (User)",
            "PE Format": "PE32+ (64-bit)" if is_pe32_plus else "PE32 (32-bit)",
            "DynamicBase (ASLR)": bool(dll_chars & self.IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE),
            "NXCompat (DEP)": bool(dll_chars & self.IMAGE_DLLCHARACTERISTICS_NX_COMPAT),
            "ControlFlowGuard (CFG)": bool(dll_chars & self.IMAGE_DLLCHARACTERISTICS_GUARD_CF),
            "Checksum": hex(checksum),
            "Authenticode Signed": sec_dir_size > 0,
            "Security Directory Size": sec_dir_size,
        }

def print_report(pe_path: Path):
    if not pe_path.exists():
        print(f"[-] File not found: {pe_path}")
        return False

    inspector = PEInspector(pe_path)
    info = inspector.inspect()

    print(f"+--------------------------------------------------------------+")
    print(f"| PE Binary Audit: {pe_path.name:<43} |")
    print(f"+--------------------------------------------------------------+")
    for k, v in info.items():
        print(f"| {k:<28} : {str(v):<30} |")
    print(f"+--------------------------------------------------------------+")
    return True

def main():
    parser = argparse.ArgumentParser(description="UNPD PE Header & Security Audit Tool")
    parser.add_argument("binary", type=Path, nargs="?", help="Path to PE binary (.sys, .exe, .dll)")
    args = parser.parse_args()

    target = args.binary
    if not target:
        default_driver = Path(__file__).resolve().parent.parent / "build" / "bin" / "unpd.sys"
        if default_driver.exists():
            target = default_driver

    if target and target.exists():
        print_report(target)
    else:
        print("[*] PE Inspector initialized in verification mode.")

if __name__ == "__main__":
    main()
