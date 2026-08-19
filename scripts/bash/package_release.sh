#!/usr/bin/env bash
# ============================================================================
# UNPD Driver Framework - Release Packaging and Checksum Generator
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BIN_DIR="${ROOT_DIR}/build/bin"
DIST_DIR="${ROOT_DIR}/dist"
VERSION="${1:-1.2.0}"

echo "========================================================"
echo " UNPD Release Packaging Utility (v${VERSION})"
echo "========================================================"

mkdir -p "${DIST_DIR}"

if [ ! -d "${BIN_DIR}" ]; then
    echo >&2 "[!] Error: Binary directory not found. Please build first."
    exit 1
fi

ARCHIVE_NAME="unpd-v${VERSION}-x64.zip"
echo "[*] Creating archive: ${DIST_DIR}/${ARCHIVE_NAME}"

TEMP_STAGE=$(mktemp -d)
cp -r "${BIN_DIR}"/* "${TEMP_STAGE}/" 2>/dev/null || true
if [ -f "${ROOT_DIR}/unpd_test_root.cer" ]; then
    cp "${ROOT_DIR}/unpd_test_root.cer" "${TEMP_STAGE}/"
fi

if command -v zip >/dev/null 2>&1; then
    (cd "${TEMP_STAGE}" && zip -r "${DIST_DIR}/${ARCHIVE_NAME}" .)
elif command -v 7z >/dev/null 2>&1; then
    7z a "${DIST_DIR}/${ARCHIVE_NAME}" "${TEMP_STAGE}/*"
else
    tar -czf "${DIST_DIR}/unpd-v${VERSION}-x64.tar.gz" -C "${TEMP_STAGE}" .
fi

rm -rf "${TEMP_STAGE}"

echo "[*] Generating SHA256 manifests..."
if command -v sha256sum >/dev/null 2>&1; then
    (cd "${DIST_DIR}" && sha256sum * > SHA256SUMS.txt)
elif command -v shasum >/dev/null 2>&1; then
    (cd "${DIST_DIR}" && shasum -a 256 * > SHA256SUMS.txt)
fi

echo "[+] Release artifacts packaged in ${DIST_DIR}:"
ls -la "${DIST_DIR}"
