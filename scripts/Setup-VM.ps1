param(
    [string]$CertPath = ".\unpd_test_root.cer"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " РќР°СЃС‚СЂРѕР№РєР° С‚РµСЃС‚РѕРІРѕР№ СЃСЂРµРґС‹ Windows 10/11 РІ VMWare        " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# 1. РџСЂРѕРІРµСЂРєР° РїСЂР°РІ Р°РґРјРёРЅРёСЃС‚СЂР°С‚РѕСЂР°
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "РЎРєСЂРёРїС‚ РґРѕР»Р¶РµРЅ Р±С‹С‚СЊ Р·Р°РїСѓС‰РµРЅ СЃ РїСЂР°РІР°РјРё РђРґРјРёРЅРёСЃС‚СЂР°С‚РѕСЂР°."
}

# 2. Р’РєР»СЋС‡РµРЅРёРµ Testsigning Рё NoIntegrityChecks
Write-Host "[*] Р’РєР»СЋС‡РµРЅРёРµ СЂРµР¶РёРјР° С‚РµСЃС‚РёСЂРѕРІР°РЅРёСЏ (TESTSIGNING)..." -ForegroundColor Yellow
& bcdedit.exe /set testsigning on
& bcdedit.exe /set nointegritychecks on
& bcdedit.exe /set bootstatuspolicy ignoreallfailures

# 3. РќР°СЃС‚СЂРѕР№РєР° РІС‹РІРѕРґР° DbgPrint РґР»СЏ DbgView / WinDbg
Write-Host "[*] РљРѕРЅС„РёРіСѓСЂР°С†РёСЏ С„РёР»СЊС‚СЂР° СЃРѕРѕР±С‰РµРЅРёР№ РѕС‚Р»Р°РґРєРё (DbgPrint)..." -ForegroundColor Yellow
$dbgPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print"
if (-not (Test-Path $dbgPath)) {
    New-Item -Path $dbgPath -Force | Out-Null
}
Set-ItemProperty -Path $dbgPath -Name "DEFAULT" -Value 0x8 -Type DWord

# 4. РРјРїРѕСЂС‚ СЃРµСЂС‚РёС„РёРєР°С‚Р°, РµСЃР»Рё РїСЂРµРґРѕСЃС‚Р°РІР»РµРЅ
if (Test-Path $CertPath) {
    Write-Host "[*] РЈСЃС‚Р°РЅРѕРІРєР° С‚РµСЃС‚РѕРІРѕРіРѕ СЃРµСЂС‚РёС„РёРєР°С‚Р° $CertPath..." -ForegroundColor Yellow
    Import-Certificate -FilePath $CertPath -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
    Import-Certificate -FilePath $CertPath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
    Write-Host "[+] РЎРµСЂС‚РёС„РёРєР°С‚ СѓСЃРїРµС€РЅРѕ СѓСЃС‚Р°РЅРѕРІР»РµРЅ РІ Root Рё TrustedPublisher." -ForegroundColor Green
} else {
    Write-Warning "Р¤Р°Р№Р» СЃРµСЂС‚РёС„РёРєР°С‚Р° $CertPath РЅРµ РЅР°Р№РґРµРЅ. Р•СЃР»Рё РґСЂР°Р№РІРµСЂ РїРѕРґРїРёСЃР°РЅ РґСЂСѓРіРёРј СЃРµСЂС‚РёС„РёРєР°С‚РѕРј, РёРјРїРѕСЂС‚РёСЂСѓР№С‚Рµ РµРіРѕ РІСЂСѓС‡РЅСѓСЋ."
}

Write-Host "`n[+] РќР°СЃС‚СЂРѕР№РєР° Р·Р°РІРµСЂС€РµРЅР°. РџРµСЂРµР·Р°РіСЂСѓР·РёС‚Рµ РІРёСЂС‚СѓР°Р»СЊРЅСѓСЋ РјР°С€РёРЅСѓ РґР»СЏ РїСЂРёРјРµРЅРµРЅРёСЏ РёР·РјРµРЅРµРЅРёР№." -ForegroundColor Green
Write-Host "    РљРѕРјР°РЅРґР° РїРµСЂРµР·Р°РіСЂСѓР·РєРё: shutdown /r /t 0" -ForegroundColor Cyan
