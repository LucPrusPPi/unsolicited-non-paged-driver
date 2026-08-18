param(
    [ValidateSet("Auto", "Clang", "MSVC")]
    [string]$Compiler = "Auto",
    [string]$Config = "Release",
    [switch]$Clean,
    [switch]$Sign
)

$ErrorActionPreference = "Stop"

$RootDir = $PSScriptRoot
Set-Location $RootDir

# Locate Visual Studio vcvars64.bat
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    $found = Get-ChildItem "C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $vcvars = $found.FullName }
}

if (-not (Test-Path $vcvars)) {
    Write-Error "Could not locate vcvars64.bat. Please install Visual Studio with C++ Desktop/WDK workload."
}

# Resolve compiler (Clang-CL or MSVC)
$clangCl = $null
if ($Compiler -eq "Auto" -or $Compiler -eq "Clang") {
    $candidates = @(
        "C:\LLVM\bin\clang-cl.exe",
        "C:\Program Files\LLVM\bin\clang-cl.exe",
        "C:\Program Files (x86)\LLVM\bin\clang-cl.exe"
    )
    foreach ($cand in $candidates) {
        if (Test-Path $cand) {
            $clangCl = $cand
            break
        }
    }
    if (-not $clangCl) {
        $cmdCheck = Get-Command "clang-cl.exe" -ErrorAction SilentlyContinue
        if ($cmdCheck) { $clangCl = $cmdCheck.Source }
    }
}

$cmakeCompilerArgs = ""
if ($Compiler -eq "Clang" -and -not $clangCl) {
    Write-Error "Clang compiler requested but clang-cl.exe was not found."
}

if ($clangCl -and ($Compiler -eq "Auto" -or $Compiler -eq "Clang")) {
    Write-Host "[*] Selected Compiler: Clang-CL ($clangCl)" -ForegroundColor Cyan
    $normalized = $clangCl.Replace('\', '/')
    $cmakeCompilerArgs = "-DCMAKE_C_COMPILER=`"$normalized`" -DCMAKE_CXX_COMPILER=`"$normalized`""
} else {
    Write-Host "[*] Selected Compiler: Microsoft Visual C++ (MSVC cl.exe)" -ForegroundColor Cyan
}

if ($Clean -and (Test-Path "build")) {
    Write-Host "[*] Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build
}

Write-Host "[*] Configuring and building project ($Config) via CMake & Ninja..." -ForegroundColor Cyan

$buildCmd = "call `"$vcvars`" && cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=$Config -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $cmakeCompilerArgs && ninja -C build"
cmd.exe /c $buildCmd

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
}

Write-Host "[+] Build completed successfully." -ForegroundColor Green

if ($Sign) {
    Write-Host "[*] Signing driver binary..." -ForegroundColor Yellow
    & powershell.exe -ExecutionPolicy Bypass -File .\scripts\Sign-Driver.ps1 -DriverPath .\build\bin\unpd.sys -ExportCert
}
