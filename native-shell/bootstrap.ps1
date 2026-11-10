[CmdletBinding()]
param(
    [string]$Workspace = "D:\SkAiBuild",
    [string]$ChromiumRef = "156.0.8078.88",
    [switch]$InitializeChromiumCheckout
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-Cmd {
    param(
        [Parameter(Mandatory)][string]$CommandLine,
        [Parameter(Mandatory)][string]$WorkingDirectory
    )
    Push-Location $WorkingDirectory
    try {
        & cmd.exe /d /s /c $CommandLine
        if ($LASTEXITCODE -ne 0) {
            throw ("Command failed with exit code {0}: {1}" -f $LASTEXITCODE, $CommandLine)
        }
    }
    finally {
        Pop-Location
    }
}

if ($env:OS -ne "Windows_NT") {
    throw "The Sk/Ai Chromium source bootstrap must run on Windows."
}
if ($Workspace -match '\s') {
    throw "Use a workspace path without spaces for the Chromium toolchain."
}
if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw "Git for Windows is required."
}

$Workspace = [IO.Path]::GetFullPath($Workspace)
New-Item -ItemType Directory -Force -Path $Workspace | Out-Null
$driveRoot = [IO.Path]::GetPathRoot($Workspace)
$driveLetter = $driveRoot.Substring(0, 1)
$volume = Get-Volume -DriveLetter $driveLetter -ErrorAction Stop
if ($volume.FileSystem -ne "NTFS") {
    throw ("The source workspace must be on NTFS. Detected: {0}" -f $volume.FileSystem)
}
if ($volume.SizeRemaining -lt 120GB) {
    throw ("The workspace volume has {0:N1} GB free. Keep at least 120 GB free." -f ($volume.SizeRemaining / 1GB))
}

$programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
$vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Installer / vswhere was not found. Install Visual Studio 2026 with Desktop development with C++ and MFC/ATL support."
}
$vsInstall = & $vswhere -latest -products '*' -version '[18.0,19.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) {
    throw "Visual Studio 2026 (18.x) with MSVC x64/x86 tools was not found."
}

$depotTools = Join-Path $Workspace "depot_tools"
$chromiumParent = Join-Path $Workspace "chromium"
$chromiumSrc = Join-Path $chromiumParent "src"

if (-not (Test-Path (Join-Path $depotTools "gclient.bat"))) {
    if (Test-Path $depotTools) {
        throw ("The depot_tools folder exists but gclient.bat is missing: {0}" -f $depotTools)
    }
    & git.exe clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $depotTools
    if ($LASTEXITCODE -ne 0) { throw "Could not clone depot_tools." }
}

$env:PATH = "$depotTools;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

if (-not (Test-Path (Join-Path $chromiumSrc ".git"))) {
    if (-not $InitializeChromiumCheckout) {
        throw ("Chromium source is missing at {0}. Re-run with -InitializeChromiumCheckout to download it." -f $chromiumSrc)
    }
    New-Item -ItemType Directory -Force -Path $chromiumParent | Out-Null
    Write-Host "Checking out upstream Chromium source. This is a large initial download." -ForegroundColor Cyan
    Invoke-Cmd -CommandLine "fetch.bat --nohooks chromium" -WorkingDirectory $chromiumParent
}

if (-not (Test-Path (Join-Path $chromiumSrc ".git"))) {
    throw ("Chromium checkout did not produce {0}." -f $chromiumSrc)
}

Write-Host ("Fetching pinned Chromium tag {0}..." -f $ChromiumRef) -ForegroundColor Cyan
& git.exe -C $chromiumSrc fetch --depth 1 origin ("refs/tags/{0}:refs/tags/{0}" -f $ChromiumRef)
if ($LASTEXITCODE -ne 0) {
    throw ("Could not fetch Chromium tag {0}." -f $ChromiumRef)
}
& git.exe -C $chromiumSrc checkout --detach $ChromiumRef
if ($LASTEXITCODE -ne 0) { throw "Could not check out the pinned Chromium tag." }

Write-Host "Synchronizing Chromium dependencies..." -ForegroundColor Cyan
Invoke-Cmd -CommandLine "gclient.bat sync --with_branch_heads --with_tags" -WorkingDirectory $chromiumSrc

$versionFile = Join-Path $chromiumSrc "chrome\VERSION"
if (-not (Test-Path $versionFile)) { throw "chrome/VERSION was not found after source synchronization." }
$versionText = Get-Content -Raw $versionFile
$versionParts = @{}
foreach ($line in ($versionText -split "\r?\n")) {
    if ($line -match '^([A-Z_]+)=(.*)$') { $versionParts[$matches[1]] = $matches[2] }
}
$actualVersion = "$($versionParts.MAJOR).$($versionParts.MINOR).$($versionParts.BUILD).$($versionParts.PATCH)"
if ($actualVersion -ne $ChromiumRef) {
    throw ("Pinned version mismatch. Requested {0}; chrome/VERSION reports {1}." -f $ChromiumRef, $actualVersion)
}

Write-Host ""
Write-Host "Sk/Ai upstream Chromium source is ready." -ForegroundColor Green
Write-Host ("Chromium: {0}" -f $ChromiumRef)
Write-Host ("Source: {0}" -f $chromiumSrc)
Write-Host ("depot_tools: {0}" -f $depotTools)
Write-Host ("Visual Studio: {0}" -f $vsInstall)
Write-Host "Next: run native-shell/build.ps1 -Workspace $Workspace -ChromiumRef $ChromiumRef -BaselineOnly"
