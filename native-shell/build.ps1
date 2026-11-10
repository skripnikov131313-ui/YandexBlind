[CmdletBinding()]
param(
    [string]$Workspace = "D:\SkAiBuild",
    [string]$ChromiumGostRef = "156.0.8078.88",
    [switch]$InitializeChromiumCheckout,
    [switch]$PrepareOnly,
    [switch]$BaselineOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$bootstrap = Join-Path $PSScriptRoot "bootstrap.ps1"
& $bootstrap -Workspace $Workspace -ChromiumGostRef $ChromiumGostRef -InitializeChromiumCheckout:$InitializeChromiumCheckout

$workspaceRoot = [IO.Path]::GetFullPath($Workspace)
$gostRepo = Join-Path $workspaceRoot "Chromium-Gost"
$chromiumSrc = Join-Path $workspaceRoot "chromium\src"
$buildDir = Join-Path $gostRepo "build_windows"
$prepareScript = Join-Path $buildDir "chromium-gost-prepare.bat"
$releaseScript = Join-Path $buildDir "chromium-gost-build-release.bat"

$env:PATH = (Join-Path $workspaceRoot "depot_tools") + ";" + $env:PATH
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

Write-Host ("Preparing pinned Chromium-Gost source {0}..." -f $ChromiumGostRef) -ForegroundColor Cyan
Push-Location $buildDir
try {
    $prepareCommand = '"' + $prepareScript + '" skai-no-shell'
    & cmd.exe /d /c $prepareCommand
    if ($LASTEXITCODE -ne 0) {
        throw ("chromium-gost-prepare.bat failed with exit code {0}" -f $LASTEXITCODE)
    }
}
finally {
    Pop-Location
}

$patchDir = Join-Path $PSScriptRoot "patches"
$patches = @(Get-ChildItem -Path $patchDir -Filter "*.patch" -File | Sort-Object Name)
if ($BaselineOnly) {
    Write-Host "BaselineOnly: skipping all Sk/Ai patches to preserve an upstream Chromium-Gost baseline." -ForegroundColor Yellow
}
else {
    if ($patches.Count -eq 0 -and -not $PrepareOnly) {
        throw "No Sk/Ai native patches exist. Refusing to label an unmodified Chromium-Gost build as Sk/Ai."
    }
    foreach ($patch in $patches) {
        Write-Host ("Checking {0}" -f $patch.Name) -ForegroundColor Cyan
        Push-Location $chromiumSrc
        try {
            & git.exe apply --check --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) { throw ("Patch check failed: {0}" -f $patch.Name) }
            & git.exe apply --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) { throw ("Patch application failed: {0}" -f $patch.Name) }
        }
        finally {
            Pop-Location
        }
    }
}
if ($PrepareOnly) {
    Write-Host "Source preparation complete. No compilation was requested." -ForegroundColor Green
    return
}

if (-not $BaselineOnly -and $patches.Count -eq 0) {
    throw "Refusing to label an unmodified Chromium-Gost build as Sk/Ai."
}

Write-Host ("Compiling {0}..." -f $ChromiumGostRef) -ForegroundColor Cyan
Push-Location $buildDir
try {
    $releaseCommand = '"' + $releaseScript + '" skai-no-shell'
    & cmd.exe /d /c $releaseCommand
    if ($LASTEXITCODE -ne 0) {
        throw ("chromium-gost-build-release.bat failed with exit code {0}" -f $LASTEXITCODE)
    }
}
finally {
    Pop-Location
}

$releaseDir = Join-Path $buildDir "RELEASE"
$zip = Get-ChildItem -Path $releaseDir -Filter "chromium-gost-$ChromiumGostRef-windows-amd64.zip" -File -ErrorAction SilentlyContinue | Select-Object -First 1
$installer = Get-ChildItem -Path $releaseDir -Filter "chromium-gost-$ChromiumGostRef-windows-amd64-installer.exe" -File -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $zip -or -not $installer) {
    throw ("Expected source-build artifacts were not found in {0}." -f $releaseDir)
}

$artifactDir = Join-Path $repoRoot "native-shell\artifacts"
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
if ($BaselineOnly) {
    Copy-Item $zip.FullName (Join-Path $artifactDir "SkAi-ChromiumGost-$ChromiumGostRef-BASELINE.zip") -Force
    Copy-Item $installer.FullName (Join-Path $artifactDir "SkAi-ChromiumGost-$ChromiumGostRef-BASELINE-Setup.exe") -Force
    Write-Warning "This is the upstream Chromium-Gost baseline, not the custom Sk/Ai browser shell."
}
else {
    Copy-Item $zip.FullName (Join-Path $artifactDir "SkAi-$ChromiumGostRef-Windows-x64.zip") -Force
    Copy-Item $installer.FullName (Join-Path $artifactDir "SkAi-$ChromiumGostRef-Windows-x64-Setup.exe") -Force
}

Write-Host ("Build artifacts copied to {0}" -f $artifactDir) -ForegroundColor Green
