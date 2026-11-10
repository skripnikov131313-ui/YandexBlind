[CmdletBinding()]
param(
    [string]$Workspace = "D:\SkAiBuild",
    [string]$ChromiumRef = "156.0.8078.88",
    [switch]$InitializeChromiumCheckout,
    [switch]$PrepareOnly,
    [switch]$BaselineOnly
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

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$bootstrap = Join-Path $PSScriptRoot "bootstrap.ps1"
& $bootstrap -Workspace $Workspace -ChromiumRef $ChromiumRef -InitializeChromiumCheckout:$InitializeChromiumCheckout

$workspaceRoot = [IO.Path]::GetFullPath($Workspace)
$chromiumSrc = Join-Path $workspaceRoot "chromium\src"
$depotTools = Join-Path $workspaceRoot "depot_tools"
$chromeVersionFile = Join-Path $chromiumSrc "chrome\VERSION"
$versionText = Get-Content -Raw $chromeVersionFile
$versionParts = @{}
foreach ($line in ($versionText -split "\r?\n")) {
    if ($line -match '^([A-Z_]+)=(.*)$') { $versionParts[$matches[1]] = $matches[2] }
}
$actualVersion = "$($versionParts.MAJOR).$($versionParts.MINOR).$($versionParts.BUILD).$($versionParts.PATCH)"
if ($actualVersion -ne $ChromiumRef) {
    throw ("Refusing to build unexpected Chromium version. Expected {0}; got {1}." -f $ChromiumRef, $actualVersion)
}

$env:PATH = "$depotTools;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"
$patchDir = Join-Path $PSScriptRoot "patches"
$patches = @(Get-ChildItem -Path $patchDir -Filter "*.patch" -File | Sort-Object Name)

if ($BaselineOnly) {
    Write-Host "BaselineOnly: building pristine upstream Chromium without Sk/Ai patches." -ForegroundColor Yellow
}
else {
    if ($patches.Count -eq 0 -and -not $PrepareOnly) {
        throw "No Sk/Ai source patches exist. Refusing to label upstream Chromium as a Sk/Ai build."
    }
    foreach ($patch in $patches) {
        Write-Host ("Checking {0}" -f $patch.Name) -ForegroundColor Cyan
        Push-Location $chromiumSrc
        try {
            & git.exe apply --check --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) {
                throw ("Patch check failed: {0}" -f $patch.Name)
            }
            & git.exe apply --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) {
                throw ("Patch application failed: {0}" -f $patch.Name)
            }
        }
        finally {
            Pop-Location
        }
    }
}

if ($PrepareOnly) {
    Write-Host "Chromium source preparation complete. No compilation was requested." -ForegroundColor Green
    return
}

$outDir = Join-Path $chromiumSrc "out\SkAi"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$argsFile = Join-Path $outDir "args.gn"
$args = @(
    'is_debug = false',
    'is_official_build = false',
    'is_component_build = false',
    'target_cpu = "x64"',
    'symbol_level = 0',
    'blink_symbol_level = 0'
)
[IO.File]::WriteAllText($argsFile, ($args -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

Write-Host "Generating GN build files..." -ForegroundColor Cyan
Invoke-Cmd -CommandLine "gn.bat gen out\SkAi" -WorkingDirectory $chromiumSrc

Write-Host "Building Chromium browser target..." -ForegroundColor Cyan
Invoke-Cmd -CommandLine "autoninja.bat -C out\SkAi chrome" -WorkingDirectory $chromiumSrc

$runtimeDir = Join-Path $repoRoot "native-shell\artifacts\runtime"
if (Test-Path $runtimeDir) { Remove-Item -Path $runtimeDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $runtimeDir | Out-Null

$binaryDir = $outDir
$runtimeFiles = @(Get-ChildItem -Path $binaryDir -File | Where-Object {
    $_.Extension -in @(".exe", ".dll", ".pak", ".dat", ".bin", ".json", ".manifest")
})
foreach ($file in $runtimeFiles) {
    Copy-Item -Path $file.FullName -Destination $runtimeDir -Force
}
foreach ($directoryName in @("locales", "resources", "swiftshader")) {
    $sourceDirectory = Join-Path $binaryDir $directoryName
    if (Test-Path $sourceDirectory -PathType Container) {
        Copy-Item -Path $sourceDirectory -Destination $runtimeDir -Recurse -Force
    }
}
if (-not (Test-Path (Join-Path $runtimeDir "chrome.exe"))) {
    throw "chrome.exe was not found in the Chromium output."
}
$label = if ($BaselineOnly) { "BASELINE" } else { "NATIVE" }
$versionNote = "Sk/Ai $label build from Chromium $ChromiumRef. This artifact is a source-build validation package."
[IO.File]::WriteAllText((Join-Path $runtimeDir "SKAI_BUILD.txt"), $versionNote + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

$artifactZip = Join-Path $repoRoot ("native-shell\artifacts\SkAi-Chromium-$ChromiumRef-$label-Windows-x64.zip")
if (Test-Path $artifactZip) { Remove-Item $artifactZip -Force }
Compress-Archive -Path (Join-Path $runtimeDir "*") -DestinationPath $artifactZip -CompressionLevel Optimal

Write-Host ("Chromium build completed. Package: {0}" -f $artifactZip) -ForegroundColor Green
if ($BaselineOnly) {
    Write-Warning "This is unmodified upstream Chromium packaged for baseline verification, not the custom Sk/Ai browser UI."
}
