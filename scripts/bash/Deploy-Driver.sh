#!/usr/bin/env bash
# ============================================================================
# UNPD Driver Framework - Service Deployment Script (Git Bash / Windows)
# ============================================================================
set -euo pipefail

ACTION="${1:-status}"
DRIVER_PATH="${2:-./build/bin/unpd.sys}"
SERVICE_NAME="${3:-unpd}"
DISPLAY_NAME="Unsolicited Non-Paged Kernel Driver"

case "${ACTION}" in
    install)
        echo "[*] Creating kernel driver service '${SERVICE_NAME}' -> ${DRIVER_PATH}"
        WIN_PATH=$(cygpath -w -a "${DRIVER_PATH}" 2>/dev/null || wslpath -w "${DRIVER_PATH}" 2>/dev/null || echo "${DRIVER_PATH}")
        sc.exe create "${SERVICE_NAME}" type= kernel binPath= "${WIN_PATH}" DisplayName= "${DISPLAY_NAME}"
        echo "[+] Service '${SERVICE_NAME}' created."
        ;;
    uninstall)
        echo "[*] Stopping and removing service '${SERVICE_NAME}'..."
        sc.exe stop "${SERVICE_NAME}" >/dev/null 2>&1 || true
        sc.exe delete "${SERVICE_NAME}"
        echo "[+] Service '${SERVICE_NAME}' removed."
        ;;
    start)
        echo "[*] Starting driver service '${SERVICE_NAME}'..."
        sc.exe start "${SERVICE_NAME}" || true
        ;;
    stop)
        echo "[*] Stopping driver service '${SERVICE_NAME}'..."
        sc.exe stop "${SERVICE_NAME}" || true
        ;;
    restart)
        echo "[*] Restarting driver service '${SERVICE_NAME}'..."
        sc.exe stop "${SERVICE_NAME}" >/dev/null 2>&1 || true
        sleep 0.5
        sc.exe start "${SERVICE_NAME}" || true
        ;;
    status)
        echo "[*] Querying service '${SERVICE_NAME}'..."
        sc.exe query "${SERVICE_NAME}" || true
        ;;
    *)
        echo "Usage: $0 [install|uninstall|start|stop|restart|status] [DriverPath] [ServiceName]"
        exit 1
        ;;
esac
