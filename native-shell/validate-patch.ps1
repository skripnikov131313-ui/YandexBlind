[CmdletBinding()]
param()
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$chromiumTag = "156.0.8078.88"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$patchPath = Join-Path $PSScriptRoot "patches\0001-skai-native-color-pipeline.patch"
if (-not (Test-Path $patchPath -PathType Leaf)) {
    throw "The first native Sk/Ai color patch is missing."
}
if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw "Git is required to validate source patches."
}

$tempRoot = Join-Path $env:TEMP ("skai-patch-check-" + [Guid]::NewGuid().ToString("N"))
$sourceRoot = Join-Path $tempRoot "src"
$colorDir = Join-Path $sourceRoot "chrome\browser\ui\color"
New-Item -ItemType Directory -Force -Path $colorDir | Out-Null

try {
    $rawBase = "https://raw.githubusercontent.com/chromium/chromium/$chromiumTag/"
    $targets = @(
        "chrome/browser/ui/color/BUILD.gn",
        "chrome/browser/ui/color/chrome_color_mixers.cc"
    )
    foreach ($relative in $targets) {
        $destination = Join-Path $sourceRoot ($relative -replace '/', '\')
        $parent = Split-Path -Parent $destination
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        $url = $rawBase + $relative
        Write-Host ("Fetching pinned source: {0}" -f $relative)
        Invoke-WebRequest -Uri $url -OutFile $destination -UseBasicParsing
    }

    & git.exe init --quiet $sourceRoot
    if ($LASTEXITCODE -ne 0) { throw "Could not initialize temporary Git workspace." }

    Push-Location $sourceRoot
    try {
        Write-Host ("Checking Sk/Ai patch against Chromium {0}..." -f $chromiumTag) -ForegroundColor Cyan
        & git.exe apply --check --whitespace=error $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw "The native Sk/Ai patch does not apply cleanly to the pinned Chromium source."
        }
    }
    finally {
        Pop-Location
    }

    Write-Host "Native Sk/Ai Chromium patch applies cleanly." -ForegroundColor Green
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -Path $tempRoot -Recurse -Force
    }
}
