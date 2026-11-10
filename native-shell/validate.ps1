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
if ($designTokens.workspace.assistantDefault -ne "docked-right") { throw "The default assistant layout must be docked-right." }
if ($designTokens.semanticColors.aiLocal -ne "mint" -or $designTokens.semanticColors.aiCloud -ne "lavender") {
    throw "AI mode semantic colors must remain Local=mint and Cloud=lavender."
}

Write-Host "Sk/Ai native-shell scaffold validation passed." -ForegroundColor Green
