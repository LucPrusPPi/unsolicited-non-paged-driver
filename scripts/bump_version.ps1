<#
.SYNOPSIS
    Automated Version Bump Script for UNPD Framework (PowerShell).
.EXAMPLE
    .\scripts\bump_version.ps1 -Version "2.5.0"
#>
param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Version
)

$ErrorActionPreference = "Stop"
$CleanVer = $Version.TrimStart("v")
$TagVer = "v$CleanVer"
$Today = (Get-Date).ToString("yyyy-MM-dd")
$RootDir = Split-Path -Parent $PSScriptRoot

Write-Host "[*] Bumping project version to $CleanVer (tag: $TagVer, date: $Today)..." -ForegroundColor Cyan

# 1. CMakeLists.txt
$CMakePath = Join-Path $RootDir "CMakeLists.txt"
if (Test-Path $CMakePath) {
    (Get-Content $CMakePath -Raw) -replace 'project\(UnsolicitedNonPagedDriver VERSION [0-9.]+', "project(UnsolicitedNonPagedDriver VERSION $CleanVer" | Set-Content $CMakePath -NoNewline
    Write-Host "  [+] Updated CMakeLists.txt" -ForegroundColor Green
}

# 2. CITATION.cff
$CffPath = Join-Path $RootDir "CITATION.cff"
if (Test-Path $CffPath) {
    $cff = (Get-Content $CffPath -Raw) -replace 'version:\s*"[^"]+"', "version: `"$CleanVer`""
    $cff = $cff -replace 'date-released:\s*"[^"]+"', "date-released: `"$Today`""
    $cff | Set-Content $CffPath -NoNewline
    Write-Host "  [+] Updated CITATION.cff" -ForegroundColor Green
}

# 3. Root vcpkg.json
$VcpkgPath = Join-Path $RootDir "vcpkg.json"
if (Test-Path $VcpkgPath) {
    (Get-Content $VcpkgPath -Raw) -replace '"version-string":\s*"[^"]+"', "`"version-string`": `"$CleanVer`"" | Set-Content $VcpkgPath -NoNewline
    Write-Host "  [+] Updated vcpkg.json" -ForegroundColor Green
}

# 4. Port vcpkg.json
$PortVcpkgPath = Join-Path $RootDir "ports\unsolicited-non-paged-driver\vcpkg.json"
if (Test-Path $PortVcpkgPath) {
    (Get-Content $PortVcpkgPath -Raw) -replace '"version-string":\s*"[^"]+"', "`"version-string`": `"$CleanVer`"" | Set-Content $PortVcpkgPath -NoNewline
    Write-Host "  [+] Updated ports/.../vcpkg.json" -ForegroundColor Green
}

# 5. Port portfile.cmake
$PortfilePath = Join-Path $RootDir "ports\unsolicited-non-paged-driver\portfile.cmake"
if (Test-Path $PortfilePath) {
    (Get-Content $PortfilePath -Raw) -replace 'REF\s*"[^"]+"', "REF `"$TagVer`"" | Set-Content $PortfilePath -NoNewline
    Write-Host "  [+] Updated ports/.../portfile.cmake" -ForegroundColor Green
}

Write-Host "[+] Version successfully bumped to $CleanVer across all manifests and build files." -ForegroundColor Cyan
