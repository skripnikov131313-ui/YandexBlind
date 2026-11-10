[CmdletBinding()]
param()
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$chromiumTag = "156.0.8078.88"
$patchDir = Join-Path $PSScriptRoot "patches"
$patches = @(Get-ChildItem -Path $patchDir -Filter "*.patch" -File | Sort-Object Name)
if ($patches.Count -eq 0) {
    throw "No native Sk/Ai source patches were found."
}
if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw "Git is required to validate source patches."
}

$tempRoot = Join-Path $env:TEMP ("skai-chromium-patch-check-" + [Guid]::NewGuid().ToString("N"))
$sourceRoot = Join-Path $tempRoot "src"
New-Item -ItemType Directory -Force -Path $sourceRoot | Out-Null
$targets = @(
    "chrome/browser/ui/color/BUILD.gn",
    "chrome/browser/ui/color/chrome_color_mixers.cc",
    "chrome/browser/ui/views/toolbar/toolbar_view.h",
    "chrome/browser/ui/views/toolbar/BUILD.gn",
    "chrome/browser/ui/views/toolbar/toolbar_view.cc",
    "chrome/browser/ui/tabs/tab_style.cc",
    "chrome/app/theme/chromium/BRANDING"
)

try {
    $rawBase = "https://raw.githubusercontent.com/chromium/chromium/$chromiumTag/"
    foreach ($relative in $targets) {
        $destination = Join-Path $sourceRoot ($relative -replace '/', '\')
        $parent = Split-Path -Parent $destination
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        Write-Host ("Fetching pinned Chromium source: {0}" -f $relative)
        Invoke-WebRequest -Uri ($rawBase + $relative) -OutFile $destination -UseBasicParsing

        # Normalize the downloaded fixture to Chromium's UTF-8 / LF checkout form.
        $sourceText = [IO.File]::ReadAllText($destination)
        $sourceText = $sourceText.TrimStart([char]0xFEFF)
        $sourceText = $sourceText.Replace("`r`n", "`n").Replace("`r", "`n")
        [IO.File]::WriteAllText($destination, $sourceText, [Text.UTF8Encoding]::new($false))
    }

    & git.exe init --quiet $sourceRoot
    if ($LASTEXITCODE -ne 0) { throw "Could not initialize temporary Git workspace." }
    & git.exe -C $sourceRoot config core.autocrlf false
    if ($LASTEXITCODE -ne 0) { throw "Could not disable line-ending conversion in patch-check workspace." }

    Push-Location $sourceRoot
    try {
        foreach ($patch in $patches) {
            Write-Host ("Checking {0} against upstream Chromium {1}..." -f $patch.Name, $chromiumTag) -ForegroundColor Cyan
            & git.exe apply --check --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) {
                throw ("The Sk/Ai patch does not apply cleanly to unmodified Chromium: {0}" -f $patch.Name)
            }
            & git.exe apply --whitespace=error $patch.FullName
            if ($LASTEXITCODE -ne 0) {
                throw ("Could not apply the Sk/Ai patch: {0}" -f $patch.Name)
            }
        }
    }
    finally {
        Pop-Location
    }

    Write-Host "All Sk/Ai patches apply cleanly to unmodified upstream Chromium." -ForegroundColor Green
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -Path $tempRoot -Recurse -Force
    }
}
