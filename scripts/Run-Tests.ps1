param(
    [string]$Config = "Release",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
Set-Location $RootDir

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " Р—Р°РїСѓСЃРє РїРѕР»РЅРѕРіРѕ С†РёРєР»Р° С‚РµСЃС‚РѕРІ UNPD                      " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. РЎР±РѕСЂРєР°
if (-not $SkipBuild) {
    Write-Host "`n[*] РЎР±РѕСЂРєР° РїСЂРѕРµРєС‚Р° (CMake + MSVC)..." -ForegroundColor Yellow
    & cmake -B build -S . -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) { Write-Error "РћС€РёР±РєР° РєРѕРЅС„РёРіСѓСЂР°С†РёРё CMake." }

    & cmake --build build --config $Config
    if ($LASTEXITCODE -ne 0) { Write-Error "РћС€РёР±РєР° РєРѕРјРїРёР»СЏС†РёРё." }
}

$DriverPath = ".\build\bin\$Config\unpd.sys"
$TestExePath = ".\build\bin\$Config\unpd_tests.exe"

if (-not (Test-Path $DriverPath)) {
    # РџСЂРѕРІРµСЂРєР° Р°Р»СЊС‚РµСЂРЅР°С‚РёРІРЅРѕРіРѕ РїСѓС‚Рё bin/
    $altDriver = ".\build\$Config\unpd.sys"
    if (Test-Path $altDriver) { $DriverPath = $altDriver }
}

if (-not (Test-Path $TestExePath)) {
    $altTest = ".\build\$Config\unpd_tests.exe"
    if (Test-Path $altTest) { $TestExePath = $altTest }
}

# 2. РџРѕРґРїРёСЃСЊ
Write-Host "`n[*] РџРѕРґРїРёСЃСЊ РґСЂР°Р№РІРµСЂР°..." -ForegroundColor Yellow
& powershell -ExecutionPolicy Bypass -File .\scripts\Sign-Driver.ps1 -DriverPath $DriverPath

# 3. Р—Р°РіСЂСѓР·РєР° СЃР»СѓР¶Р±С‹
Write-Host "`n[*] Р—Р°РіСЂСѓР·РєР° РґСЂР°Р№РІРµСЂР° СЏРґСЂР°..." -ForegroundColor Yellow
& powershell -ExecutionPolicy Bypass -File .\scripts\Deploy-Driver.ps1 -Action start -DriverPath $DriverPath

try {
    # 4. Р—Р°РїСѓСЃРє С‚РµСЃС‚РѕРІ
    Write-Host "`n[*] Р’С‹РїРѕР»РЅРµРЅРёРµ РЅР°Р±РѕСЂР° С‚РµСЃС‚РѕРІ..." -ForegroundColor Cyan
    & $TestExePath
    $testExit = $LASTEXITCODE
} finally {
    # 5. Р’С‹РіСЂСѓР·РєР° СЃР»СѓР¶Р±С‹
    Write-Host "`n[*] Р’С‹РіСЂСѓР·РєР° РґСЂР°Р№РІРµСЂР° СЏРґСЂР°..." -ForegroundColor Yellow
    & powershell -ExecutionPolicy Bypass -File .\scripts\Deploy-Driver.ps1 -Action stop
}

if ($testExit -eq 0) {
    Write-Host "`n[+] Р’СЃРµ С‚РµСЃС‚С‹ СѓСЃРїРµС€РЅРѕ РїСЂРѕР№РґРµРЅС‹!" -ForegroundColor Green
} else {
    Write-Error "`n[!] РўРµСЃС‚С‹ Р·Р°РІРµСЂС€РёР»РёСЃСЊ СЃ РѕС€РёР±РєР°РјРё (Exit code: $testExit)."
}
