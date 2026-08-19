#!/usr/bin/env bash
# ============================================================================
# UNPD Driver Framework - Authenticode Signing Script (Git Bash)
# ============================================================================
set -euo pipefail

DRIVER_PATH="${1:-./build/bin/unpd.sys}"
CERT_NAME="Microsoft Windows Driver Test Publisher (UNPD)"

echo "[*] Signing driver binary: ${DRIVER_PATH}"

SIGNTOOL=""
for tool in \
    "/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/signtool.exe" \
    "/c/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/signtool.exe" \
    "/c/Program Files (x86)/Windows Kits/10/bin/x64/signtool.exe"; do
    if [[ -f "${tool}" ]]; then
        SIGNTOOL="${tool}"
        break
    fi
done

if [[ -z "${SIGNTOOL}" ]]; then
    SIGNTOOL="signtool.exe"
fi

WIN_DRIVER=$(cygpath -w -a "${DRIVER_PATH}" 2>/dev/null || echo "${DRIVER_PATH}")

echo "[*] Invoking SignTool..."
"${SIGNTOOL}" sign /v /s "MY" /n "${CERT_NAME}" /fd sha256 /tr "http://timestamp.digicert.com" /td sha256 "${WIN_DRIVER}" || \
"${SIGNTOOL}" sign /v /s "MY" /n "${CERT_NAME}" /fd sha256 "${WIN_DRIVER}"

echo "[+] Verifying driver digital signature..."
"${SIGNTOOL}" verify /v /kp "${WIN_DRIVER}" || true

echo "[+] Driver signing step completed."
