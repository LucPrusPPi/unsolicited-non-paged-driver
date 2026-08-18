param(
    [ValidateSet("start", "stop", "restart", "install", "uninstall", "status")]
    [string]$Action = "status",
    [string]$DriverPath = ".\build\bin\Release\unpd.sys",
    [string]$ServiceName = "unpd",
    [string]$DisplayName = "Unsolicited Non-Paged Kernel Driver"
)

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "Р”Р»СЏ СѓРїСЂР°РІР»РµРЅРёСЏ СЃР»СѓР¶Р±Р°РјРё СЏРґСЂР° С‚СЂРµР±СѓСЋС‚СЃСЏ РїСЂР°РІР° РђРґРјРёРЅРёСЃС‚СЂР°С‚РѕСЂР°."
}

function Get-ServiceState($name) {
    $svc = Get-Service -Name $name -ErrorAction SilentlyContinue
    if ($svc) { return $svc.Status }
    return "NotInstalled"
}

switch ($Action) {
    "install" {
        $fullDriverPath = (Resolve-Path $DriverPath).Path
        Write-Host "[*] РЎРѕР·РґР°РЅРёРµ СЃР»СѓР¶Р±С‹ РґСЂР°Р№РІРµСЂР° '$ServiceName' -> $fullDriverPath" -ForegroundColor Cyan
        & sc.exe create $ServiceName type= kernel binPath= "$fullDriverPath" DisplayName= "$DisplayName"
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[+] РЎР»СѓР¶Р±Р° '$ServiceName' СѓСЃРїРµС€РЅРѕ СЃРѕР·РґР°РЅР°." -ForegroundColor Green
        }
    }

    "uninstall" {
        Write-Host "[*] РћСЃС‚Р°РЅРѕРІРєР° Рё СѓРґР°Р»РµРЅРёРµ СЃР»СѓР¶Р±С‹ '$ServiceName'..." -ForegroundColor Yellow
        & sc.exe stop $ServiceName | Out-Null
        & sc.exe delete $ServiceName
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[+] РЎР»СѓР¶Р±Р° '$ServiceName' СѓСЃРїРµС€РЅРѕ СѓРґР°Р»РµРЅР°." -ForegroundColor Green
        }
    }

    "start" {
        $state = Get-ServiceState $ServiceName
        if ($state -eq "NotInstalled") {
            $fullDriverPath = (Resolve-Path $DriverPath).Path
            Write-Host "[*] РЎР»СѓР¶Р±Р° РЅРµ РЅР°Р№РґРµРЅР°, СЃРѕР·РґР°РµРј..." -ForegroundColor Yellow
            & sc.exe create $ServiceName type= kernel binPath= "$fullDriverPath" DisplayName= "$DisplayName"
        }
        Write-Host "[*] Р—Р°РїСѓСЃРє СЃР»СѓР¶Р±С‹ '$ServiceName'..." -ForegroundColor Cyan
        & sc.exe start $ServiceName
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[+] РЎР»СѓР¶Р±Р° '$ServiceName' СѓСЃРїРµС€РЅРѕ Р·Р°РїСѓС‰РµРЅР°." -ForegroundColor Green
        }
    }

    "stop" {
        Write-Host "[*] РћСЃС‚Р°РЅРѕРІРєР° СЃР»СѓР¶Р±С‹ '$ServiceName'..." -ForegroundColor Yellow
        & sc.exe stop $ServiceName
    }

    "restart" {
        Write-Host "[*] РџРµСЂРµР·Р°РїСѓСЃРє СЃР»СѓР¶Р±С‹ '$ServiceName'..." -ForegroundColor Yellow
        & sc.exe stop $ServiceName | Out-Null
        Start-Sleep -Milliseconds 500
        & sc.exe start $ServiceName
    }

    "status" {
        $state = Get-ServiceState $ServiceName
        Write-Host "РЎС‚Р°С‚СѓСЃ СЃР»СѓР¶Р±С‹ '$ServiceName': $state" -ForegroundColor Cyan
        & sc.exe query $ServiceName
    }
}
