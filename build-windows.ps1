<#
.SYNOPSIS
    Build (and install) the Dean Amp VST3 on Windows.

.DESCRIPTION
    Configures the CMake project with the newest installed Visual Studio,
    builds the VST3 (and optionally the Standalone) in Release, then copies the
    plugin into the system VST3 folder so your DAW can find it.

    First run downloads JUCE via CMake FetchContent (one-off, can take a few
    minutes). Subsequent runs are incremental and fast.

.PARAMETER NoInstall
    Build only; do not copy the VST3 into the system folder.

.PARAMETER Standalone
    Also build the Standalone .exe (handy for testing without a DAW).

.PARAMETER VstDir
    Override the install location (default: %CommonProgramFiles%\VST3).

.PARAMETER Clean
    Delete the build-win folder first for a clean configure.

.EXAMPLE
    .\build-windows.ps1
    .\build-windows.ps1 -Standalone
    .\build-windows.ps1 -NoInstall
    .\build-windows.ps1 -VstDir "D:\VstPlugins\VST3"
#>
[CmdletBinding()]
param(
    [switch] $NoInstall,
    [switch] $Standalone,
    [string] $VstDir = (Join-Path $env:CommonProgramFiles 'VST3'),
    [switch] $Clean
)

$ErrorActionPreference = 'Stop'
$ProjectDir = $PSScriptRoot
$BuildDir   = Join-Path $ProjectDir 'build-win'
$ProductName = 'Dean Amp'                 # PRODUCT_NAME in CMakeLists.txt

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Fail($msg)       { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

# --- Prerequisites ----------------------------------------------------------
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Fail "CMake is not on your PATH. Install it from https://cmake.org/download/ (tick 'Add to PATH'), then re-run."
}

Set-Location $ProjectDir

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "Cleaning $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

# --- Configure --------------------------------------------------------------
# Try the newest VS automatically first; fall back to explicit generators.
function Invoke-Configure {
    $attempts = @(
        @('-A','x64'),                                  # newest installed VS, 64-bit
        @('-G','Visual Studio 17 2022','-A','x64'),     # VS 2022
        @('-G','Visual Studio 16 2019','-A','x64'),     # VS 2019
        @()                                             # whatever CMake defaults to
    )
    foreach ($a in $attempts) {
        Write-Step ("Configuring: cmake -S . -B build-win " + ($a -join ' '))
        & cmake -S . -B $BuildDir @a
        if ($LASTEXITCODE -eq 0) { return $true }
        Write-Host "  (that generator did not work, trying the next option...)" -ForegroundColor DarkYellow
    }
    return $false
}

if (-not (Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))) {
    if (-not (Invoke-Configure)) {
        Fail "CMake configure failed. Make sure Visual Studio (with the 'Desktop development with C++' workload) is installed."
    }
}

# --- Build ------------------------------------------------------------------
$targets = @('DeanAmp_VST3')
if ($Standalone) { $targets += 'DeanAmp_Standalone' }

foreach ($t in $targets) {
    Write-Step "Building $t (Release)"
    & cmake --build $BuildDir --config Release --target $t --parallel
    if ($LASTEXITCODE -ne 0) { Fail "Build of $t failed." }
}

# --- Locate the built VST3 --------------------------------------------------
$vst3 = Get-ChildItem -Path $BuildDir -Recurse -Directory -Filter "$ProductName.vst3" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\Release\\' } |
        Select-Object -First 1
if (-not $vst3) {
    $vst3 = Get-ChildItem -Path $BuildDir -Recurse -Directory -Filter "$ProductName.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $vst3) { Fail "Build succeeded but I couldn't find '$ProductName.vst3' under $BuildDir." }
Write-Host "Built: $($vst3.FullName)" -ForegroundColor Green

if ($Standalone) {
    $exe = Get-ChildItem -Path $BuildDir -Recurse -Filter "$ProductName.exe" -ErrorAction SilentlyContinue |
           Where-Object { $_.FullName -match '\\Standalone\\' } | Select-Object -First 1
    if ($exe) { Write-Host "Standalone: $($exe.FullName)" -ForegroundColor Green }
}

# --- Install ----------------------------------------------------------------
if ($NoInstall) {
    Write-Step "Skipping install (-NoInstall). Point your DAW at the path above, or copy the .vst3 folder into your VST3 directory."
    exit 0
}

$dest = Join-Path $VstDir "$ProductName.vst3"
Write-Step "Installing to $dest"

# Copies the whole .vst3 bundle (it's a folder on Windows). The system VST3
# folder lives under Program Files, which needs admin — so do the copy from an
# elevated PowerShell if a plain copy is denied.
$copyScript = @"
`$ErrorActionPreference='Stop'
if (-not (Test-Path -LiteralPath '$VstDir')) { New-Item -ItemType Directory -Force -Path '$VstDir' | Out-Null }
if (Test-Path -LiteralPath '$dest') { Remove-Item -LiteralPath '$dest' -Recurse -Force }
Copy-Item -LiteralPath '$($vst3.FullName)' -Destination '$VstDir' -Recurse -Force
"@

try {
    if (-not (Test-Path -LiteralPath $VstDir)) { New-Item -ItemType Directory -Force -Path $VstDir | Out-Null }
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Copy-Item -LiteralPath $vst3.FullName -Destination $VstDir -Recurse -Force
    Write-Host "Installed (no elevation needed)." -ForegroundColor Green
}
catch {
    # Most likely the system VST3 folder needs admin rights. Retry the copy in
    # an elevated PowerShell (a single UAC prompt). Any other error surfaces there too.
    Write-Host "Plain copy failed ($($_.Exception.Message)). Requesting administrator rights for the install (accept the UAC prompt)..." -ForegroundColor Yellow
    $b64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($copyScript))
    $p = Start-Process powershell -Verb RunAs -Wait -PassThru -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',$b64
    if ($p.ExitCode -ne 0) { Fail "Elevated copy failed (exit $($p.ExitCode)). You can also copy '$($vst3.FullName)' into '$VstDir' manually." }
    Write-Host "Installed (elevated)." -ForegroundColor Green
}

Write-Step "Done."
Write-Host "Rescan plugins in your DAW. Look for 'Dean Amp' by 'Dean Audio' (Fx | Distortion)." -ForegroundColor Green
