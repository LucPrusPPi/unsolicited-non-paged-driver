# VMWare Workstation Test Environment Setup

Guide for setting up a Windows 10 or Windows 11 x64 virtual machine in VMWare Workstation for kernel driver testing and WinDbg KDNET debugging.

---

## 1. Virtual Machine Preparation

1. Create a virtual machine with Windows 10 x64 (21H2/22H2) or Windows 11 x64 in VMWare Workstation.
2. Disable Secure Boot in VM Settings under Options -> Advanced -> Firmware Type by unchecking Enable Secure Boot.
3. Install VMware Tools to enable file drag-and-drop and shared clipboard.

---

## 2. Enabling Test Mode and Importing Certificates

Test-signing mode allows Windows 64-bit kernels to load binaries signed by custom test certificates.

1. Copy the following files into the guest VM:
   - `build/bin/unpd.sys`
   - `build/bin/unpd_tests.exe`
   - `unpd_test_root.cer`
   - `scripts/Setup-VM.ps1`
   - `scripts/Deploy-Driver.ps1`
   - `scripts/Run-Tests.ps1`
2. Open PowerShell as Administrator inside the VM and execute:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Setup-VM.ps1 -CertPath .\unpd_test_root.cer
   ```
3. Reboot the VM:
   ```powershell
   shutdown /r /t 0
   ```
4. Verify that the "Test Mode" watermark appears in the lower-right corner of the Windows desktop.

---

## 3. Running Automated Tests

Inside the VM, execute the test runner from an Administrator PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\Run-Tests.ps1 -SkipBuild
```

The script registers the `unpd` kernel service via the Service Control Manager, starts the driver, runs the `unpd_tests.exe` test suite, and unloads the driver upon test completion.

---

## 4. Kernel Debugging via WinDbg KDNET

To enable interactive network kernel debugging between your host machine and the guest VM:

1. Inside the guest VM (Admin Command Prompt), configure the KDNET connection:
   ```cmd
   bcdedit /debug on
   bcdedit /dbgsettings net hostip:<HOST_IP> port:50000 key:1.2.3.4
   ```
2. Open WinDbg on the host machine, select File -> Attach to Kernel -> Net, and enter port 50000 and key 1.2.3.4.
3. Reboot the guest VM. WinDbg will establish a connection during kernel boot.

---

## 5. Capturing Kernel Output via DebugView

To view `DbgPrint` and `KdPrint` messages without attaching WinDbg:

1. Download Sysinternals DebugView (`Dbgview.exe`).
2. Run DebugView as Administrator.
3. Enable Capture -> Capture Kernel and Capture -> Enable Verbose Kernel Output.
