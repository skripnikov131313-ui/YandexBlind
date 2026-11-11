# Sk/Ai Windows x64 package

## Install (current Windows user only)

1. Extract the complete package to a writable folder.
2. Open PowerShell in that folder.
3. Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install-SkAi.ps1`.

Add `-DesktopShortcut` to create a desktop shortcut. Installation uses `%LOCALAPPDATA%\Programs\SkAi`, creates a Start menu shortcut and registers a per-user uninstall entry. Administrator elevation is not required.

## Uninstall

Close Sk/Ai and run `Uninstall-SkAi.ps1` from the installed folder, or use Windows Installed Apps. Uninstall removes program files and shortcuts only. It deliberately preserves `%LOCALAPPDATA%\SkAi`, including browser profiles, cookies, history, settings and downloaded AI models.

The Qwen model is not bundled; Sk/Ai downloads it through its in-app model installer and verifies SHA-256 before use.
