[CmdletBinding()]
param()
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$required = @(
    "README.md",
    "ARCHITECTURE.md",
    "BUILD.md",
    "bootstrap.ps1",
    "build.ps1",
    "patches\README.md",
    "design\tokens.json"
)
foreach ($relative in $required) {
    $path = Join-Path $PSScriptRoot $relative
    if (-not (Test-Path $path -PathType Leaf)) {
        throw ("Required native-shell file missing: {0}" -f $relative)
    }
}

foreach ($scriptName in @("bootstrap.ps1", "build.ps1", "validate.ps1", "validate-patch.ps1")) {
    $path = Join-Path $PSScriptRoot $scriptName
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -gt 0) {
        throw ("PowerShell parse error in {0}: {1}" -f $scriptName, $errors[0].Message)
    }
}

$tokensJson = Join-Path $PSScriptRoot "design\tokens.json"
$designTokens = Get-Content -Raw $tokensJson | ConvertFrom-Json
if ($designTokens.product -ne "Sk/Ai") { throw "Unexpected design token product name." }

$manifestPath = Join-Path $PSScriptRoot "..\newtab\manifest.json"
$manifest = Get-Content -Raw $manifestPath | ConvertFrom-Json
if (-not $manifest.key) { throw "The Sk/Ai extension must have a stable public manifest key." }
if ($manifest.side_panel.default_path -ne "sidepanel.html") {
    throw "The manifest must register the Sk/Ai Assistant side panel."
}
$keyBytes = [Convert]::FromBase64String($manifest.key)
$sha256 = [Security.Cryptography.SHA256]::Create()
try { $digest = $sha256.ComputeHash($keyBytes) } finally { $sha256.Dispose() }
$idBuilder = [Text.StringBuilder]::new()
for ($i = 0; $i -lt 16; $i++) {
    [void]$idBuilder.Append([char](97 + (($digest[$i] -shr 4) -band 15)))
    [void]$idBuilder.Append([char](97 + ($digest[$i] -band 15)))
}
$derivedExtensionId = $idBuilder.ToString()
if ($derivedExtensionId -ne "ggpogknlgefbmnlahkjmchoioahcpmkk") {
    throw ("Stable Sk/Ai extension ID mismatch: {0}" -f $derivedExtensionId)
}
if ($designTokens.workspace.assistantDefault -ne "docked-right") { throw "The default assistant layout must be docked-right." }
if ($designTokens.semanticColors.aiLocal -ne "mint" -or $designTokens.semanticColors.aiCloud -ne "lavender") {
    throw "AI mode semantic colors must remain Local=mint and Cloud=lavender."
}

Write-Host "Sk/Ai native-shell scaffold validation passed." -ForegroundColor Green
