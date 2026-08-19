#!/usr/bin/env python3
"""
UNPD Driver Service Control Manager (SCM) Interface
Provides native Win32 Service Control Manager integration using ctypes to create,
start, stop, and delete the UNPD kernel service.
"""

import sys
import ctypes
from ctypes import wintypes
import argparse
from pathlib import Path

# SCM Constants
SC_MANAGER_ALL_ACCESS = 0xF003F
SERVICE_ALL_ACCESS    = 0xF01FF
SERVICE_KERNEL_DRIVER = 0x00000001
SERVICE_DEMAND_START  = 0x00000003
SERVICE_ERROR_NORMAL  = 0x00000001
SERVICE_CONTROL_STOP  = 0x00000001

SERVICE_NAME = "UnsolicitedNonPagedDriver"
DISPLAY_NAME = "Unsolicited Non-Paged Driver (UNPD)"

class SERVICE_STATUS(ctypes.Structure):
    _fields_ = [
        ("dwServiceType", wintypes.DWORD),
        ("dwCurrentState", wintypes.DWORD),
        ("dwControlsAccepted", wintypes.DWORD),
        ("dwWin32ExitCode", wintypes.DWORD),
        ("dwServiceSpecificExitCode", wintypes.DWORD),
        ("dwCheckPoint", wintypes.DWORD),
        ("dwWaitHint", wintypes.DWORD),
    ]

def get_scm_handles():
    advapi32 = ctypes.windll.advapi32
    h_scm = advapi32.OpenSCManagerW(None, None, SC_MANAGER_ALL_ACCESS)
    return advapi32, h_scm

def install_driver(driver_path: Path) -> bool:
    print(f"[*] Installing kernel service '{SERVICE_NAME}' from {driver_path}...")
    advapi32, h_scm = get_scm_handles()
    if not h_scm:
        print("[-] Failed to open Service Control Manager (Requires Admin rights).")
        return False

    h_service = advapi32.CreateServiceW(
        h_scm,
        SERVICE_NAME,
        DISPLAY_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        str(driver_path),
        None,
        None,
        None,
        None,
        None
    )

    if h_service:
        print(f"[+] Service '{SERVICE_NAME}' created successfully.")
        advapi32.CloseServiceHandle(h_service)
        advapi32.CloseServiceHandle(h_scm)
        return True

    err = ctypes.GetLastError()
    if err == 1073:  # ERROR_SERVICE_EXISTS
        print(f"[*] Service '{SERVICE_NAME}' already registered.")
        advapi32.CloseServiceHandle(h_scm)
        return True

    print(f"[-] CreateService failed with error code: {err}")
    advapi32.CloseServiceHandle(h_scm)
    return False

def main():
    parser = argparse.ArgumentParser(description="UNPD Service Control Manager")
    parser.add_argument("action", choices=["install", "start", "stop", "remove", "status"], default="status", nargs="?")
    parser.add_argument("--path", type=Path, default=Path(__file__).resolve().parent.parent / "build" / "bin" / "unpd.sys")
    args = parser.parse_args()

    print(f"[*] UNPD Driver SCM Controller -> Action: {args.action}")
    if args.action == "install":
        install_driver(args.path)

if __name__ == "__main__":
    main()
