[CmdletBinding()]
param([switch]$DesktopShortcut)

$ErrorActionPreference = "Stop"
$sourceRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$installRoot = Join-Path $env:LOCALAPPDATA "Programs\SkAi"
$startMenuFolder = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\SkAi"
$desktopShortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "SkAi.lnk"
$registryPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SkAi"
$requiredFiles = @(
    "SkAi.exe", "SkAi.dll", "libcef.dll", "chrome_elf.dll",
    "icudtl.dat", "resources.pak", "ui\index.html",
    "ui\assistant.html", "runtime\llama\llama-server.exe",
    "Install-SkAi.ps1", "Uninstall-SkAi.ps1"
)

foreach ($relativePath in $requiredFiles) {
    if (!(Test-Path -LiteralPath (Join-Path $sourceRoot $relativePath) -PathType Leaf)) {
        throw "Sk/Ai package is incomplete. Missing: $relativePath"
    }
}

if (Get-Process -Name "SkAi" -ErrorAction SilentlyContinue) {
    throw "Close Sk/Ai before installing or updating it. The running browser was not stopped."
}

$robocopy = Join-Path $env:SystemRoot "System32\robocopy.exe"
if (!(Test-Path -LiteralPath $robocopy)) { throw "Windows Robocopy is unavailable." }
New-Item -ItemType Directory -Force -Path $installRoot | Out-Null
& $robocopy $sourceRoot $installRoot /MIR /R:2 /W:1 /NFL /NDL /NP /NJH /NJS
if ($LASTEXITCODE -ge 8) { throw "Could not copy Sk/Ai files. Robocopy exit code: $LASTEXITCODE" }

$exePath = Join-Path $installRoot "SkAi.exe"
if (!(Test-Path -LiteralPath $exePath -PathType Leaf)) { throw "Sk/Ai executable was not installed." }
$versionPath = Join-Path $installRoot "VERSION.txt"
$version = if (Test-Path -LiteralPath $versionPath) { (Get-Content -LiteralPath $versionPath -Raw).Trim() } else { "0.3.0" }
if ([string]::IsNullOrWhiteSpace($version)) { $version = "0.3.0" }

New-Item -ItemType Directory -Force -Path $startMenuFolder | Out-Null
$wsh = New-Object -ComObject WScript.Shell
$startMenuLink = $wsh.CreateShortcut((Join-Path $startMenuFolder "Sk-Ai.lnk"))
$startMenuLink.TargetPath = $exePath
$startMenuLink.WorkingDirectory = $installRoot
$startMenuLink.IconLocation = "$exePath,0"
$startMenuLink.Description = "Sk/Ai Browser"
$startMenuLink.Save()

if ($DesktopShortcut) {
    $desktopLink = $wsh.CreateShortcut($desktopShortcutPath)
    $desktopLink.TargetPath = $exePath
    $desktopLink.WorkingDirectory = $installRoot
    $desktopLink.IconLocation = "$exePath,0"
    $desktopLink.Description = "Sk/Ai Browser"
    $desktopLink.Save()
}

New-Item -Path $registryPath -Force | Out-Null
$uninstallExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"
$uninstallScript = Join-Path $installRoot "Uninstall-SkAi.ps1"
$uninstallCommand = '"' + $uninstallExe + '" -NoProfile -ExecutionPolicy Bypass -File "' + $uninstallScript + '"'
Set-ItemProperty -Path $registryPath -Name "DisplayName" -Value "Sk/Ai Browser"
Set-ItemProperty -Path $registryPath -Name "DisplayVersion" -Value $version
Set-ItemProperty -Path $registryPath -Name "Publisher" -Value "Sk/Ai"
Set-ItemProperty -Path $registryPath -Name "InstallLocation" -Value $installRoot
Set-ItemProperty -Path $registryPath -Name "DisplayIcon" -Value $exePath
Set-ItemProperty -Path $registryPath -Name "UninstallString" -Value $uninstallCommand
Set-ItemProperty -Path $registryPath -Name "NoModify" -Type DWord -Value 1
Set-ItemProperty -Path $registryPath -Name "NoRepair" -Type DWord -Value 1

Write-Host "Sk/Ai $version installed for the current Windows user."
Write-Host "Application: $exePath"
Write-Host "User data and downloaded models in %LOCALAPPDATA%\SkAi were not modified."
