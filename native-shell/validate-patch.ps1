[CmdletBinding()]
param()
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$chromiumTag = "156.0.8078.88"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$patchDir = Join-Path $PSScriptRoot "patches"
$patches = @(Get-ChildItem -Path $patchDir -Filter "*.patch" -File | Sort-Object Name)
if ($patches.Count -eq 0) {
    throw "No native Sk/Ai source patches were found."
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
        "chrome/browser/ui/color/chrome_color_mixers.cc",
        "chrome/app/theme/chromium/BRANDING"
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
        foreach ($patch in $patches) {
            Write-Host ("Checking {0} against Chromium {1}..." -f $patch.Name, $chromiumTag) -ForegroundColor Cyan
            & git.exe apply --check --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) {
                throw ("The native Sk/Ai patch does not apply cleanly: {0}" -f $patch.Name)
            }
            & git.exe apply --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) {
                throw ("The native Sk/Ai patch could not be applied: {0}" -f $patch.Name)
            }
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
