[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$installRoot = Join-Path $env:LOCALAPPDATA "Programs\SkAi"
$expectedRoot = [System.IO.Path]::GetFullPath($installRoot)
$startMenuFolder = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\SkAi"
$desktopShortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "SkAi.lnk"
$registryPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SkAi"

if ((Get-Process -Name "SkAi" -ErrorAction SilentlyContinue)) {
    throw "Close Sk/Ai before uninstalling it. The running browser was not stopped."
}
if (Test-Path -LiteralPath $registryPath) {
    $registeredLocation = (Get-ItemProperty -Path $registryPath -Name "InstallLocation" -ErrorAction SilentlyContinue).InstallLocation
    if ($registeredLocation -and [System.IO.Path]::GetFullPath($registeredLocation) -ne $expectedRoot) {
        throw "The uninstall record points to a different directory. No files were removed."
    }
}

if (Test-Path -LiteralPath $startMenuFolder) { Remove-Item -LiteralPath $startMenuFolder -Recurse -Force }
if (Test-Path -LiteralPath $desktopShortcutPath) { Remove-Item -LiteralPath $desktopShortcutPath -Force }
if (Test-Path -LiteralPath $registryPath) { Remove-Item -LiteralPath $registryPath -Recurse -Force }
if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force }

Write-Host "Sk/Ai program files and shortcuts were removed."
Write-Host "Browser profiles, history, cookies, settings and downloaded models in %LOCALAPPDATA%\SkAi were preserved."
