param(
    [string]$DriverPath = ".\build\bin\Release\unpd.sys",
    [string]$CertSubject = "CN=Microsoft Windows Driver Test Publisher (UNPD)",
    [string]$CertStore = "Cert:\CurrentUser\My",
    [switch]$ExportCert
)

$ErrorActionPreference = "Stop"

$SignToolPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
if (-not (Test-Path $SignToolPath)) {
    $found = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $SignToolPath = $found.FullName
    } else {
        Write-Error "signtool.exe не найден. Проверьте установку Windows SDK/WDK."
    }
}

Write-Host "[*] Поиск тестового сертификата: $CertSubject..." -ForegroundColor Cyan
$cert = Get-ChildItem -Path $CertStore | Where-Object { $_.Subject -eq $CertSubject } | Select-Object -First 1

if (-not $cert) {
    Write-Host "[+] Создание нового тестового сертификата подписи кода..." -ForegroundColor Yellow
    $cert = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $CertSubject `
        -KeyUsage DigitalSignature `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -CertStoreLocation $CertStore `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3") `
        -NotAfter (Get-Date).AddYears(5)
    
    Write-Host "[+] Сертификат создан с отпечатком: $($cert.Thumbprint)" -ForegroundColor Green

    # Добавляем в Trusted Root и Trusted Publisher для локального запуска
    try {
        $rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "LocalMachine")
        $rootStore.Open("ReadWrite")
        $rootStore.Add($cert)
        $rootStore.Close()

        $pubStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "LocalMachine")
        $pubStore.Open("ReadWrite")
        $pubStore.Add($cert)
        $pubStore.Close()
        Write-Host "[+] Сертификат успешно установлен в Root и TrustedPublisher локальной машины." -ForegroundColor Green
    } catch {
        Write-Warning "Для добавления сертификата в доверенные корневые центры локальной машины требуются права Администратора."
    }
}

if ($ExportCert) {
    $certExportPath = ".\unpd_test_root.cer"
    Export-Certificate -Cert $cert -FilePath $certExportPath -Force | Out-Null
    Write-Host "[+] Публичный сертификат экспортирован в $certExportPath (перенесите его на VM)" -ForegroundColor Green
}

if (-not (Test-Path $DriverPath)) {
    Write-Warning "Файл драйвера $DriverPath не найден. Выполните сборку перед подписью."
    exit 0
}

Write-Host "[*] Подпись файла драйвера: $DriverPath..." -ForegroundColor Cyan
& $SignToolPath sign /v /s "MY" /n "Microsoft Windows Driver Test Publisher (UNPD)" /fd sha256 /tr "http://timestamp.digicert.com" /td sha256 $DriverPath

if ($LASTEXITCODE -ne 0) {
    Write-Host "[*] Повторная подпись без таймстемпа..." -ForegroundColor Yellow
    & $SignToolPath sign /v /s "MY" /n "Microsoft Windows Driver Test Publisher (UNPD)" /fd sha256 $DriverPath
}

Write-Host "[+] Проверка цифровой подписи..." -ForegroundColor Cyan
& $SignToolPath verify /v /kp $DriverPath

Write-Host "[+] Драйвер успешно подписан." -ForegroundColor Green
