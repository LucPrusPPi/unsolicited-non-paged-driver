param(
    [string]$CertPath = ".\unpd_test_root.cer"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " UNPD VM Test Environment Setup (Windows 10/11)          " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Check Administrator Privileges
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "This script must be executed with Administrator privileges."
}

# 2. Enable Testsigning and disable integrity checks
Write-Host "[*] Enabling TESTSIGNING and ignoring integrity checks..." -ForegroundColor Yellow
& bcdedit.exe /set testsigning on
& bcdedit.exe /set nointegritychecks on
& bcdedit.exe /set bootstatuspolicy ignoreallfailures

# 3. Configure DbgPrint filter in registry
Write-Host "[*] Configuring kernel debug print filter (DbgPrint)..." -ForegroundColor Yellow
$dbgPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print"
if (-not (Test-Path $dbgPath)) {
    New-Item -Path $dbgPath -Force | Out-Null
}
Set-ItemProperty -Path $dbgPath -Name "DEFAULT" -Value 0x8 -Type DWord

# 4. Import test certificate if present
if (Test-Path $CertPath) {
    Write-Host "[*] Installing test root certificate $CertPath..." -ForegroundColor Yellow
    Import-Certificate -FilePath $CertPath -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
    Import-Certificate -FilePath $CertPath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
    Write-Host "[+] Certificate successfully installed into Root and TrustedPublisher." -ForegroundColor Green
} else {
    Write-Warning "Certificate file $CertPath not found."
}

Write-Host "`n[+] VM configuration complete. Reboot the virtual machine to apply changes." -ForegroundColor Green
Write-Host "    Reboot command: shutdown /r /t 0" -ForegroundColor Cyan
