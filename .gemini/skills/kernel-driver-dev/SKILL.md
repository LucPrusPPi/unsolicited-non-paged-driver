---
name: kernel-driver-dev
description: >-
  Workflow and automation guide for building, signing, deploying to VMWare,
  and testing Windows Kernel Drivers (WDM/KMDF) in testmode.
---

# Windows Kernel Driver Development Workflow

## Overview
Workflow for developing, compiling with WDK, signing with custom X.509 test certificates, deploying as a Windows Kernel Service, and running IOCTL verification/stress tests in a VMWare test environment.

## Quick Start

### 1. Build Driver and Test Suite
```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### 2. Sign Driver
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Sign-Driver.ps1 -DriverPath .\build\bin\Release\unpd.sys
```

### 3. Deploy and Run Tests
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-Tests.ps1
```

## Workflow Steps

### Step 1: Environment Setup on Host / VM
- Enable Test Signing on VM:
  ```cmd
  bcdedit /set testsigning on
  bcdedit /set nointegritychecks on
  ```
- Import generated root certificate to `Trusted Root Certification Authorities` and `Trusted Publishers`.

### Step 2: Driver Architecture
- Memory allocations: `ExAllocatePool2(POOL_FLAG_NON_PAGED, size, tag)`
- Dispatch routines: IRP_MJ_CREATE, IRP_MJ_CLOSE, IRP_MJ_DEVICE_CONTROL
- Buffer probing: Wrap `METHOD_NEITHER` in `__try` / `__except` with `ProbeForRead` / `ProbeForWrite`.

### Step 3: Service Management
- Create Service: `sc create unpd type= kernel binPath= C:\path\to\unpd.sys`
- Start Service: `sc start unpd`
- Stop Service: `sc stop unpd`
- Delete Service: `sc delete unpd`
