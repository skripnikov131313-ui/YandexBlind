# Sk/Ai

Sk/Ai is being developed as a standalone Windows 11 x64 desktop browser. The native browser shell is a C++20 Win32 application embedding Chromium through CEF, with a custom Aurora Glass interface and an assistant workspace.

## Current development branches

- `feature/skai-native-shell`: the active native browser implementation. It includes the CEF/Win32 shell, native tab controller, address/search routing, navigation controls, docked assistant UI and a separate native assistant popup.
- `feature/sk-ai-privacy-security`: the earlier Chromium extension-based prototype. It is retained as a reference for the previous assistant and privacy experiments; it is not the native browser runtime.

The native shell is still under development. Source implementation, successful Windows compilation, Windows runtime testing and public release are tracked separately. Do not treat the current prototype as a released or fully functional daily browser.

## Product direction

- A custom Sk/Ai browser window, tab strip, toolbar, address/search bar and session restoration for the last 50 HTTP(S) tabs.
- Standard Chromium rendering and security through CEF.
- Yandex, Google, Bing, DuckDuckGo, Brave Search and Startpage as selectable search providers.
- Local bookmark folders with editable titles, browsing history and a persistent downloads library; downloads are stored in `Downloads/SkAi`.
- An assistant docked at the right side of the browser, detachable into a separate topmost Windows window and redockable.
- Soft Aurora visual language: dark graphite/navy, indigo-violet and mint/teal accents, with a coordinated light theme.
- 8 GB RAM target and CPU-only local AI.
- No project analytics SDK or telemetry endpoint.
- Local AI planned around Qwen 3.5 2B GGUF Q4_K_M and llama.cpp. It is not connected in the native shell yet.
- Cloud AI remains a safe placeholder. No prompt, page content or file is transmitted.
- Standard public PKI/TLS and Chromium security. Russian GOST TLS and CryptoPro CSP are explicitly out of scope.

## Native implementation

See [browser-shell/ARCHITECTURE.md](browser-shell/ARCHITECTURE.md) for the CEF bootstrap architecture and Windows build instructions, and [browser-shell/PRODUCT-SPEC.md](browser-shell/PRODUCT-SPEC.md) for the full acceptance checklist.

The Windows Actions workflow downloads the pinned CEF package, verifies its archive hash, builds the native shell, checks the runtime package and performs a native-window smoke test. The project is not considered built until that workflow succeeds and the app is tested on Windows 11.

## Hardware target

The target is Windows 11 x64 with 8 GB system RAM and no discrete GPU requirement. The Qwen 2B model is a planned compromise for short everyday text tasks; CPU speed and available memory will affect response time. Model integration and memory profiling remain open work.

## Privacy model

- No project analytics SDK or silent project telemetry.
- Local mode must not send prompts or page content outside the device.
- Cloud mode must remain disabled until an explicit provider is configured and the user approves exactly what will be transferred.
- Chromium certificate validation, sandbox and Safe Browsing must remain enabled.
- Website requests and requests to the selected search provider still expose normal network metadata such as the user's IP address.

## Third-party components

CEF/Chromium, llama.cpp and Qwen are third-party components. Their upstream licenses and notices apply. Verify the model repository's current license and include required notices before redistribution.
