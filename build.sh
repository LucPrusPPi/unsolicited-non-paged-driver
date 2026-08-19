#!/usr/bin/env bash
# ============================================================================
# UNPD Driver Framework - Dual-Compiler 1-Click Build Script (Git Bash / WSL)
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG="Release"
COMPILER="Auto"
CLEAN=0
SIGN=0
SKIP_TESTS=0

usage() {
    echo "Usage: ./build.sh [-c Config] [-k Compiler] [--clean] [--sign] [--skip-tests]"
    echo "  -c, --config      Build configuration: Debug | Release (default: Release)"
    echo "  -k, --compiler    Target compiler: Clang | MSVC | Auto (default: Auto)"
    echo "  --clean           Clean build directory before configuring"
    echo "  --sign            Digitally sign the driver binary using test certificate"
    echo "  --skip-tests      Skip running GoogleTest validation suite after build"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--config) CONFIG="$2"; shift 2 ;;
        -k|--compiler) COMPILER="$2"; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        --sign) SIGN=1; shift ;;
        --skip-tests) SKIP_TESTS=1; shift ;;
        -h|--help) usage ;;
        *) echo "[!] Unknown parameter: $1"; usage ;;
    esac
done

echo "========================================================"
echo " UNPD Dual-Compiler Build Runner (Git Bash / POSIX)"
echo " Configuration: ${CONFIG}"
echo " Compiler:      ${COMPILER}"
echo "========================================================"

BUILD_DIR="${SCRIPT_DIR}/build"

if [[ ${CLEAN} -eq 1 ]]; then
    echo "[*] Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if command -v ninja >/dev/null 2>&1; then
    cmake -G Ninja -DCMAKE_BUILD_TYPE="${CONFIG}" "${SCRIPT_DIR}"
    ninja
else
    cmake -DCMAKE_BUILD_TYPE="${CONFIG}" "${SCRIPT_DIR}"
    cmake --build . --config "${CONFIG}"
fi

echo "[+] Build completed successfully."

# Signing step
if [[ ${SIGN} -eq 1 ]]; then
    if [[ -f "${SCRIPT_DIR}/scripts/bash/Sign-Driver.sh" ]]; then
        bash "${SCRIPT_DIR}/scripts/bash/Sign-Driver.sh" "${BUILD_DIR}/bin/unpd.sys"
    elif [[ -f "${SCRIPT_DIR}/scripts/powershell/Sign-Driver.ps1" ]]; then
        powershell.exe -ExecutionPolicy Bypass -File "${SCRIPT_DIR}/scripts/powershell/Sign-Driver.ps1" -DriverPath "${BUILD_DIR}/bin/unpd.sys"
    fi
fi

# Run tests
if [[ ${SKIP_TESTS} -eq 0 ]]; then
    if [[ -f "${BUILD_DIR}/bin/unpd_tests.exe" ]]; then
        echo "[*] Executing GoogleTest validation harness..."
        "${BUILD_DIR}/bin/unpd_tests.exe"
    elif [[ -f "${BUILD_DIR}/bin/unpd_tests" ]]; then
        echo "[*] Executing GoogleTest validation harness..."
        "${BUILD_DIR}/bin/unpd_tests"
    fi
fi

echo "[+] Process finished."
