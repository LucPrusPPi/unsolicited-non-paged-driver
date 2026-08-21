#!/usr/bin/env python3
"""
Automated Version Bump Script for UNPD Framework.
Synchronizes version across CMakeLists.txt, CITATION.cff, vcpkg manifests, portfiles, and documentation.
Usage: python scripts/bump_version.py <new_version>
Example: python scripts/bump_version.py 2.5.0
"""

import sys
import re
from pathlib import Path
from datetime import datetime

def bump_version(new_version: str):
    root = Path(__file__).resolve().parent.parent
    today = datetime.now().strftime("%Y-%m-%d")
    clean_ver = new_version.lstrip("v")
    tag_ver = f"v{clean_ver}"

    print(f"[*] Bumping project version to {clean_ver} (tag: {tag_ver}, date: {today})...")

    # 1. CMakeLists.txt
    cmake_path = root / "CMakeLists.txt"
    if cmake_path.exists():
        content = cmake_path.read_text(encoding="utf-8")
        content = re.sub(r'project\(UnsolicitedNonPagedDriver VERSION [0-9.]+', f'project(UnsolicitedNonPagedDriver VERSION {clean_ver}', content)
        cmake_path.write_text(content, encoding="utf-8")
        print(f"  [+] Updated {cmake_path.name}")

    # 2. CITATION.cff
    cff_path = root / "CITATION.cff"
    if cff_path.exists():
        content = cff_path.read_text(encoding="utf-8")
        content = re.sub(r'version:\s*"[^"]+"', f'version: "{clean_ver}"', content)
        content = re.sub(r'date-released:\s*"[^"]+"', f'date-released: "{today}"', content)
        cff_path.write_text(content, encoding="utf-8")
        print(f"  [+] Updated {cff_path.name}")

    # 3. Root vcpkg.json
    vcpkg_path = root / "vcpkg.json"
    if vcpkg_path.exists():
        content = vcpkg_path.read_text(encoding="utf-8")
        content = re.sub(r'"version-string":\s*"[^"]+"', f'"version-string": "{clean_ver}"', content)
        vcpkg_path.write_text(content, encoding="utf-8")
        print(f"  [+] Updated {vcpkg_path.name}")

    # 4. Port vcpkg.json
    port_vcpkg_path = root / "ports" / "unsolicited-non-paged-driver" / "vcpkg.json"
    if port_vcpkg_path.exists():
        content = port_vcpkg_path.read_text(encoding="utf-8")
        content = re.sub(r'"version-string":\s*"[^"]+"', f'"version-string": "{clean_ver}"', content)
        port_vcpkg_path.write_text(content, encoding="utf-8")
        print(f"  [+] Updated ports/.../{port_vcpkg_path.name}")

    # 5. Port portfile.cmake
    portfile_path = root / "ports" / "unsolicited-non-paged-driver" / "portfile.cmake"
    if portfile_path.exists():
        content = portfile_path.read_text(encoding="utf-8")
        content = re.sub(r'REF\s*"[^"]+"', f'REF "{tag_ver}"', content)
        portfile_path.write_text(content, encoding="utf-8")
        print(f"  [+] Updated ports/.../{portfile_path.name}")

    print(f"[+] Version successfully bumped to {clean_ver} across all manifests and build files.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python scripts/bump_version.py <version>")
        sys.exit(1)
    bump_version(sys.argv[1])
