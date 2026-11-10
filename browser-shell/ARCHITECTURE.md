# Sk/Ai Native Shell — Architecture and Build

## Product architecture

- Product target: standalone Windows 11 x64 browser with a custom C++20 Win32 shell.
- Rendering engine: Chromium embedded through CEF. GOST TLS, Russian root-certificate overrides and CryptoPro are out of scope.
- Native UI: Aurora Glass interface from `ui/`, rendered by a dedicated CEF browser.
- Web contents: separate CEF browser instances per native tab. Native controller owns tab IDs, active state, bounds and lifetime.
- Assistant: docked in the main UI; detach creates a separate native CEF popup with Windows topmost behavior. The popup can be closed to redock.
- Local AI: Qwen GGUF through a separately managed llama.cpp service, planned but not integrated.
- Cloud AI: placeholder only; no provider is connected and no prompts/page content are sent.
- Persistence: browser profile is located under `%LOCALAPPDATA%/SkAi/Profiles/Default`. Assistant conversation persistence exists in UI source but cross-window and cross-session semantics still require runtime verification.

## CEF version and sandbox

The build pins CEF `154.0.34+g14c5a08+chromium-154.0.8037.98`.

CEF 138+ Windows client builds use the bootstrap architecture. Sk/Ai is built as `SkAi.dll`; the matching `bootstrap.exe` is copied as `SkAi.exe`. The bootstrap calls the exported `RunWinMain` entry point and supplies the sandbox handle. The client passes that handle to `CefExecuteProcess` and `CefInitialize` and rejects startup if the handle is missing. The build does not silently disable the Chromium sandbox.

## Build locally on Windows

Requirements:
- Windows 11 x64.
- Visual Studio 2022 with the C++ desktop workload and a Windows SDK supported by the pinned CEF package.
- CMake 3.24 or newer.
- Matching CEF 154 Windows x64 binary distribution extracted locally.

PowerShell:

```powershell
$env:CEF_ROOT = "C:\path\to\cef_binary_154.0.34+g14c5a08+chromium-154.0.8037.98_windows64"

cmake -S browser-shell -B out/skai -A x64 -DCEF_ROOT="$env:CEF_ROOT" -DUSE_SANDBOX=ON
cmake --build out/skai --config Release --parallel 2
```

The packaged output is written to `out/skai/bin/Release`. It contains `SkAi.exe` (CEF bootstrap), `SkAi.dll` (Sk/Ai client), CEF runtime DLLs, resource packs, `locales/` and the local `ui/` directory.

## CI

`.github/workflows/native-shell-windows.yml` downloads the pinned CEF archive, verifies its published SHA-1, runs Node syntax checks, configures and builds the native target, checks required runtime files, starts the application for a native-window smoke test, then uploads `skai-win-x64-runtime`.

A green CI run and an actual Windows 11 test are required before calling the browser built or usable. The current Actions job is queued; no successful build is being claimed.

## Product boundaries

The existing extension-based branch is retained as a separate prototype/reference. It is not the runtime or UI shell for this native CEF branch.
