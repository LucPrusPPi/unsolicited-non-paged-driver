#!/usr/bin/env bash
# ============================================================================
# UNPD Driver Framework - VM Test Environment Setup (Git Bash)
# ============================================================================
set -euo pipefail

CERT_PATH="${1:-./unpd_test_root.cer}"

echo "========================================================"
echo " UNPD VM Environment Setup (Git Bash / Windows Target)"
echo "========================================================"

# Enable test-signing and disable integrity checks
echo "[*] Enabling TESTSIGNING and ignoring integrity checks..."
bcdedit.exe /set testsigning on
bcdedit.exe /set nointegritychecks on
bcdedit.exe /set bootstatuspolicy ignoreallfailures

# Configure DbgPrint filter in registry
echo "[*] Enabling kernel debug print outputs..."
reg.exe add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print" /v DEFAULT /t REG_DWORD /d 8 /f || true

# Import root certificate if present
if [[ -f "${CERT_PATH}" ]]; then
    echo "[*] Importing test publisher certificate: ${CERT_PATH}"
    WIN_CERT=$(cygpath -w -a "${CERT_PATH}" 2>/dev/null || echo "${CERT_PATH}")
    certutil.exe -addstore "Root" "${WIN_CERT}" || true
    certutil.exe -addstore "TrustedPublisher" "${WIN_CERT}" || true
    echo "[+] Certificate imported into Root and TrustedPublisher stores."
fi

echo "[+] VM configuration complete. Reboot VM with: shutdown /r /t 0"
