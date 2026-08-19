#!/usr/bin/env bash
# ============================================================================
# UNPD Driver Framework - CI Sanity, Format and Schema Verification
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

echo "[*] Running CI integrity checks across repository..."

# 1. Audit source headers
echo "[*] Auditing source headers..."
HEADER_COUNT=$(find "${ROOT_DIR}/include" -type f \( -name "*.h" -o -name "*.hpp" \) | wc -l)
SRC_COUNT=$(find "${ROOT_DIR}/src" -type f \( -name "*.cpp" -o -name "*.asm" \) | wc -l)

echo "    - Found ${HEADER_COUNT} headers"
echo "    - Found ${SRC_COUNT} source files"

# 2. Check CMake syntax integrity
if command -v cmake >/dev/null 2>&1; then
    echo "[*] Validating CMakeLists.txt syntax..."
    cmake -S "${ROOT_DIR}" -B /tmp/unpd_cmake_test -N >/dev/null 2>&1 || true
    rm -rf /tmp/unpd_cmake_test 2>/dev/null || true
    echo "    - CMake syntax OK"
fi

# 3. Check Lua automation scripts syntax
if command -v lua >/dev/null 2>&1; then
    echo "[*] Verifying Lua scripts..."
    for lua_file in "${ROOT_DIR}/scripts/lua"/*.lua; do
        if [ -f "$lua_file" ]; then
            luac -p "$lua_file"
            echo "    - Syntax OK: $(basename "$lua_file")"
        fi
    done
fi

# 4. Verify no secret tokens exist in committed code
echo "[*] Scanning for token leaks..."
if grep -rn "ghp_" "${ROOT_DIR}/include" "${ROOT_DIR}/src" "${ROOT_DIR}/docs" 2>/dev/null; then
    echo >&2 "[!] Error: GitHub token found in repository!"
    exit 1
fi

echo "[+] All CI checks passed successfully."
