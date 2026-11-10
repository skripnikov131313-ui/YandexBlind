# Sk/Ai

**Sk/Ai is a native Windows browser project.** The goal is an original browser application with its own window chrome, tabs, omnibox, workspace, assistant, privacy controls, settings and installer.

Upstream Chromium is the internal web engine. Sk/Ai owns the product interface and browser experience; Chromium supplies HTML/CSS rendering, JavaScript execution, networking and mature web-platform behavior. The target is not a Chromium reskin or an extension as the final product.

## Design direction

The current UI direction is the supplied Soft Aurora design:
- graphite / indigo base with lavender, cyan and mint accents;
- light and dark modes following Windows by default;
- native top tabs and navigation row;
- vertical left navigation rail;
- a main web workspace;
- an assistant docked on the right, not a modal overlay;
- Local AI in mint and Cloud AI in lavender;
- assistant can later detach into a separate native window.

## Current source track

The native browser work is developed in [native-shell/](native-shell/README.md) on the `feature/sk-ai-native-shell` branch.

- Chromium source pin: 156.0.8078.88 from the upstream Chromium repository.
- Native UI patch series: ColorProvider palette, Sk/Ai branding, tab/toolbar geometry and an initial assistant toolbar entry point.
- Source bootstrap/build tooling uses depot_tools, GN and Ninja.
- The first standalone UI preview is in [native-shell/ui-prototype/](native-shell/ui-prototype/README.md).

The UI preview is interactive HTML/CSS/JavaScript; it is not yet the final native browser shell. The C++ patches currently have exact-source apply-checks but have not yet been compiled into a Sk/Ai browser.

## AI direction

- Local target: Qwen 3.5 2B GGUF Q4_K_M with a CPU inference runtime.
- Runtime target: Windows 11 x64 with 8 GB RAM and no mandatory discrete GPU.
- Local AI listens only on loopback and receives page context after explicit user action.
- Cloud AI is not enabled until provider selection, secret storage and explicit per-request data consent exist.
- Changing the Local / Cloud switch must not silently upload messages, files or page content.

## Security and privacy

- Standard upstream Chromium TLS and certificate validation with Windows platform certificate stores.
- No GOST TLS, msspi or CryptoPro CSP requirement.
- Keep Chromium sandboxing, site isolation, Safe Browsing and certificate interstitials enabled.
- No project analytics SDK or telemetry endpoint.
- Site permissions for microphone, camera, geolocation and notifications remain user-controlled.
- Page text and uploaded files are untrusted context; secrets and form values are not included in AI prompts by default.

## Build constraints

A complete Chromium source build needs a dedicated Windows build host. Current upstream instructions require at least 100 GB free NTFS disk, recommend more than 16 GB RAM, and specify Visual Studio 2026 with the C++ desktop workload. The 8 GB everyday computer is the runtime target, not the preferred build machine.

See [native-shell/BUILD.md](native-shell/BUILD.md) for the reproducible source checkout and build process.

## Legacy proof of concept

The `newtab/` extension and previous installer scripts are retained as a reference for interaction ideas and early AI experiments. They are not the target architecture and should not be presented as the finished Sk/Ai browser.
