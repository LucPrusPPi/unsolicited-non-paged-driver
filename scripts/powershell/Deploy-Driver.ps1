param(
    [ValidateSet("start", "stop", "restart", "install", "uninstall", "status")]
    [string]$Action = "status",
    [string]$DriverPath = ".\build\bin\unpd.sys",
    [string]$ServiceName = "unpd",
    [string]$DisplayName = "Unsolicited Non-Paged Kernel Driver"
)

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "Admin privileges required for kernel service operations."
}

function Get-ServiceState($name) {
    $svc = Get-Service -Name $name -ErrorAction SilentlyContinue
    if ($svc) { return $svc.Status }
    return "NotInstalled"
}

switch ($Action) {
    "install" {
        $fullDriverPath = (Resolve-Path $DriverPath).Path
        Write-Host "[*] Creating driver service '$ServiceName' -> $fullDriverPath" -ForegroundColor Cyan
        & sc.exe create $ServiceName type= kernel binPath= "$fullDriverPath" DisplayName= "$DisplayName"
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[+] Service '$ServiceName' created successfully." -ForegroundColor Green
        }
    }

    "uninstall" {
        Write-Host "[*] Stopping and removing service '$ServiceName'..." -ForegroundColor Yellow
        & sc.exe stop $ServiceName | Out-Null
        & sc.exe delete $ServiceName
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[+] Service '$ServiceName' deleted successfully." -ForegroundColor Green
        }
    }

    "start" {
        $state = Get-ServiceState $ServiceName
        if ($state -eq "NotInstalled") {
            $fullDriverPath = (Resolve-Path $DriverPath).Path
            Write-Host "[*] Service not found, creating..." -ForegroundColor Yellow
            & sc.exe create $ServiceName type= kernel binPath= "$fullDriverPath" DisplayName= "$DisplayName"
        }
        Write-Host "[*] Starting service '$ServiceName'..." -ForegroundColor Cyan
        & sc.exe start $ServiceName
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[+] Service '$ServiceName' started successfully." -ForegroundColor Green
        }
    }

    "stop" {
        Write-Host "[*] Stopping service '$ServiceName'..." -ForegroundColor Yellow
        & sc.exe stop $ServiceName
    }

    "restart" {
        Write-Host "[*] Restarting service '$ServiceName'..." -ForegroundColor Yellow
        & sc.exe stop $ServiceName | Out-Null
        Start-Sleep -Milliseconds 500
        & sc.exe start $ServiceName
    }

    "status" {
        $state = Get-ServiceState $ServiceName
        Write-Host "Service '$ServiceName' status: $state" -ForegroundColor Cyan
        & sc.exe query $ServiceName
    }
}
