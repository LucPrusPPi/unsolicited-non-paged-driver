param(
    [string]$Config = "Release",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $RootDir

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " UNPD Full Test Pipeline Execution                      " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Build
if (-not $SkipBuild) {
    Write-Host "`n[*] Building project with build.ps1..." -ForegroundColor Yellow
    & powershell -ExecutionPolicy Bypass -File .\build.ps1 -Config $Config -Sign
}

$DriverPath = ".\build\bin\unpd.sys"
$TestExePath = ".\build\bin\unpd_tests.exe"

if (-not (Test-Path $DriverPath)) {
    $altDriver = ".\build\bin\$Config\unpd.sys"
    if (Test-Path $altDriver) { $DriverPath = $altDriver }
}

if (-not (Test-Path $TestExePath)) {
    $altTest = ".\build\bin\$Config\unpd_tests.exe"
    if (Test-Path $altTest) { $TestExePath = $altTest }
}

# 2. Sign
Write-Host "`n[*] Signing driver binary..." -ForegroundColor Yellow
& powershell -ExecutionPolicy Bypass -File .\scripts\powershell\Sign-Driver.ps1 -DriverPath $DriverPath

# 3. Start Service
Write-Host "`n[*] Loading kernel driver service..." -ForegroundColor Yellow
& powershell -ExecutionPolicy Bypass -File .\scripts\powershell\Deploy-Driver.ps1 -Action start -DriverPath $DriverPath

try {
    # 4. Run Tests
    Write-Host "`n[*] Executing GoogleTest test suite..." -ForegroundColor Cyan
    & $TestExePath
    $testExit = $LASTEXITCODE
} finally {
    # 5. Stop Service
    Write-Host "`n[*] Unloading kernel driver service..." -ForegroundColor Yellow
    & powershell -ExecutionPolicy Bypass -File .\scripts\powershell\Deploy-Driver.ps1 -Action stop
}

if ($testExit -eq 0) {
    Write-Host "`n[+] All tests passed successfully!" -ForegroundColor Green
} else {
    Write-Error "`n[!] Tests failed with exit code: $testExit"
}
