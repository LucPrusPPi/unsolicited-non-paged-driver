param(
    [string]$Config = "Release",
    [switch]$Clean,
    [switch]$Sign
)

$ErrorActionPreference = "Stop"

$RootDir = $PSScriptRoot
Set-Location $RootDir

$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    $found = Get-ChildItem "C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $vcvars = $found.FullName }
}

if ($Clean -and (Test-Path "build")) {
    Write-Host "[*] Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build
}

Write-Host "[*] Configuring and building project ($Config)..." -ForegroundColor Cyan

$buildCmd = 'call "' + $vcvars + '" && cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=' + $Config + ' && ninja -C build'
cmd.exe /c $buildCmd

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
}

Write-Host "[+] Build completed successfully." -ForegroundColor Green

if ($Sign) {
    Write-Host "[*] Signing driver binary..." -ForegroundColor Yellow
    & powershell.exe -ExecutionPolicy Bypass -File .\scripts\Sign-Driver.ps1 -DriverPath .\build\bin\unpd.sys -ExportCert
}
