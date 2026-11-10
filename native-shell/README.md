# Sk/Ai Native Browser

This is the native browser product track. It is intentionally separate from the earlier extension-based proof of concept.

## Product boundary

Sk/Ai owns the Windows browser application, window chrome, tabs, navigation controls, workspace, assistant, preferences, privacy UX, installer and update experience.

**Upstream Chromium is the embedded browser engine and the foundation for mature web compatibility.** We do not write a new HTML/CSS renderer, JavaScript engine or network stack. We modify Chromium's browser application and UI source so that the product visible to the user is Sk/Ai, not the stock Chromium interface.

This source track uses ordinary upstream Chromium. There is no Chromium-Gost dependency, GOST TLS layer, msspi submodule or CryptoPro CSP requirement.

## Pinned source baseline

- Upstream repository: https://chromium.googlesource.com/chromium/src
- Chromium tag: **156.0.8078.88**
- Local source path on the build machine: D:\SkAiBuild\chromium\src
- depot_tools: D:\SkAiBuild\depot_tools

The exact Chromium version is read from chrome/VERSION after checkout and verified before patching or building. Version changes require an explicit reviewed update to the source pin and patch series.

## Browser window target

The default layout follows the supplied Sk/Ai design:

1. A native Windows title area with Sk/Ai identity and native window controls.
2. A custom tab strip with new-tab button.
3. A custom navigation row: Back, Forward, Reload, secure connection indicator, omnibox, bookmarks, downloads, profile and menu.
4. A left navigation rail.
5. The active page/search workspace.
6. A right-side Sk/Ai Assistant dock that consumes its own layout width; it never covers the webpage.

The assistant is designed to support:
- docking on the right by default, with optional left docking;
- resizing through a draggable divider;
- detached mode in a separate resizable native window;
- minimize, maximize, restore and re-dock;
- optional Windows Always-on-Top, off by default;
- retained chat state when switching tabs or window mode.

## Native implementation

The actual shell belongs in Chromium C++ Views / browser UI, not in an extension page.

Primary integration points for Chromium 156:
- chrome/browser/ui/views/frame/browser_view.* — browser window layout and content host.
- chrome/browser/ui/views/frame/browser_view_layout.* — layout calculations.
- chrome/browser/ui/views/frame/tab_strip_region_view.* — tab strip host.
- chrome/browser/ui/views/toolbar/toolbar_view.* — navigation controls and toolbar.
- chrome/browser/ui/views/side_panel/* — native docked panel infrastructure.
- chrome/browser/ui/color/* — ColorProvider mixers for Soft Aurora.
- chrome/browser/ui/tabs/tab_style.* — native tab geometry.

The browser's existing Browser, Profile, TabStripModel, WebContents, LocationBarModel, navigation controller, permission manager, download manager, security-state services and session restore remain canonical. Sk/Ai replaces their presentation and user-facing workflow, not their security-critical implementations.

## AI boundary

- Local Qwen runs as a separate CPU inference process bound to 127.0.0.1 only.
- Page context is transferred only after explicit user action.
- Password inputs, payment fields, cookies, hidden form values and browser profile data are never scraped into prompts.
- Page text and uploaded files are untrusted context, not instructions.
- Cloud mode remains unavailable until a provider, secure secret storage, data disclosure and per-request confirmation are implemented.
- Switching Local / Cloud must never silently transmit chat history or page context.

## Privacy and security

- Use upstream Chromium certificate verification and platform certificate stores.
- Use standard TLS. No GOST TLS or CryptoPro integration is in scope.
- Do not disable certificate validation, sandboxing, site isolation or Safe Browsing to work around compatibility problems.
- Avoid project analytics SDKs and project telemetry endpoints.
- Permissions for microphone, camera, geolocation, notifications and clipboard remain user-controlled.
- Browser privacy controls should expose clear behavior for cookies, third-party storage, trackers and site permissions without falsifying website consent or breaking first-party sessions.

## Windows source build

A full Chromium source build is a separate build-host operation. The user's 8 GB Windows PC is the runtime target, not the recommended source-build machine. Chromium's current Windows instructions list at least 100 GB free NTFS disk and recommend more than 16 GB RAM; they require Visual Studio 2026 with the C++ desktop workload and MFC/ATL components.

Use the bootstrap and build scripts:

    pwsh -File .\native-shell\bootstrap.ps1 -Workspace D:\SkAiBuild -ChromiumRef 156.0.8078.88 -InitializeChromiumCheckout

    pwsh -File .\native-shell\build.ps1 -Workspace D:\SkAiBuild -ChromiumRef 156.0.8078.88 -BaselineOnly

The baseline build compiles unmodified upstream Chromium. To build the Sk/Ai patch overlay, omit -BaselineOnly after the ordered patch series passes validation.

A configured self-hosted runner can be labelled:

    self-hosted, windows, x64, skai-chromium-large

A hosted Windows runner is used for script checks, JavaScript syntax validation and patch apply-checks; it is not used for the complete source build.

## UI prototype

The supplied design is implemented as a standalone interactive preview in [ui-prototype/](ui-prototype/). Open ui-prototype/index.html in a desktop browser. No framework, package installation or external image/font request is required.

The prototype demonstrates layout and interaction only. It does not render websites, connect Qwen or ship as the browser shell. The next step is to port its visual system into native Chromium Views and connect it to real browser services.

## Current implementation state

- [x] Separate native-browser source branch.
- [x] Clean upstream Chromium source pin.
- [x] Source bootstrap and GN/Ninja build scripts.
- [x] Exact-source patch validator against upstream Chromium 156.0.8078.88.
- [x] Soft Aurora ColorProvider patch.
- [x] Sk/Ai product branding patch.
- [x] Rounded tab and toolbar-spacing patch.
- [x] Native assistant toolbar entry point (opens the Sk/Ai extension side-panel entry when that extension is installed).
- [x] Interactive HTML/CSS/JS UI prototype matching the supplied reference.
- [ ] Full clean Chromium source checkout on a large Windows build host.
- [ ] Compile pristine Chromium baseline.
- [ ] Compile Sk/Ai patch overlay.
- [ ] Replace the stock browser frame and native tab strip with the full Sk/Ai layout.
- [ ] Implement the real docked assistant view inside the browser-owned workspace.
- [ ] Floating assistant window and Always-on-Top integration.
- [ ] Local Qwen bridge, profile integration and runtime validation on Windows 11.

A passing JavaScript check or patch apply-check does not mean Chromium has compiled. Source preparation, compilation and runtime tests are tracked separately.
