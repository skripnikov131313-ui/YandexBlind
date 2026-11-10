# Sk/Ai Native Shell — Architecture and Build

- Product target: a standalone Windows 11 x64 browser with a custom C++20 Win32 shell.
- Rendering engine: standard Chromium through CEF. GOST and CryptoPro are not in scope.
- UI: the Aurora Glass interface under `ui/`.
- Local AI: Qwen GGUF through a separately managed llama.cpp service, planned but not integrated.
- Cloud AI and Internet Search are separate assistant modes. Cloud transmission requires explicit consent; Internet Search uses the selected search provider.
- Assistant layout: docked at the right, collapsible and detachable; Windows always-on-top is a native window feature, not a CSS effect.
- Persistence: chat/session state must survive Local / Cloud / Internet Search mode changes.

## Build

Requires Windows x64, Visual Studio 2022 C++ workload, CMake 3.24+, and an extracted matching CEF binary distribution. Set `CEF_ROOT`, then run:

```powershell
cmake -S browser-shell -B out/skai -A x64 -DCEF_ROOT="$env:CEF_ROOT"
cmake --build out/skai --config Release
```

CEF is not vendored. This initial scaffold has not yet been built or tested on Windows. The next engineering milestone is wiring the UI address bar, navigation buttons and tabs to a native CEF browser controller, then adding CEF subprocess/resource/sandbox packaging and a proper borderless window.
