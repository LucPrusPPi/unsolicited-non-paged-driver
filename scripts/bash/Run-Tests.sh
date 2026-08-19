#!/usr/bin/env bash
# ============================================================================
# UNPD Driver Framework - End-to-End Test Pipeline Runner (Git Bash)
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CONFIG="${1:-Release}"
SKIP_BUILD="${2:-0}"

echo "========================================================"
echo " UNPD Full Test Pipeline Runner (Git Bash)"
echo " Configuration: ${CONFIG}"
echo "========================================================"

if [[ "${SKIP_BUILD}" != "1" && "${SKIP_BUILD}" != "--skip-build" ]]; then
    echo "[*] Building driver and test binaries..."
    bash "${ROOT_DIR}/build.sh" --config "${CONFIG}" --sign --skip-tests
fi

DRIVER_PATH="${ROOT_DIR}/build/bin/unpd.sys"
TEST_EXE="${ROOT_DIR}/build/bin/unpd_tests.exe"

if [[ ! -f "${TEST_EXE}" ]]; then
    TEST_EXE="${ROOT_DIR}/build/bin/unpd_tests"
fi

# Start driver service
echo "[*] Loading kernel driver..."
bash "${SCRIPT_DIR}/Deploy-Driver.sh" start "${DRIVER_PATH}" || true

TEST_STATUS=0
echo "[*] Executing test harness..."
if [[ -f "${TEST_EXE}" ]]; then
    "${TEST_EXE}" || TEST_STATUS=$?
else
    echo "[!] Test executable not found: ${TEST_EXE}"
    TEST_STATUS=1
fi

# Stop driver service
echo "[*] Unloading kernel driver..."
bash "${SCRIPT_DIR}/Deploy-Driver.sh" stop "${DRIVER_PATH}" || true

if [[ ${TEST_STATUS} -eq 0 ]]; then
    echo "[+] All tests completed successfully."
else
    echo "[!] Tests failed with exit code: ${TEST_STATUS}"
    exit ${TEST_STATUS}
fi
