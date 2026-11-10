# Sk/Ai — Product Specification

This file is the project-wide acceptance checklist. A feature is not considered complete merely because its control is visible in the UI: it must be implemented, tested and documented.

## Product goal
A standalone Windows 11 x64 desktop browser with its own Sk/Ai visual language, native window shell, Chromium rendering engine, privacy controls and integrated AI workspace. The browser must be usable as a normal daily browser, not only as an AI launcher.

## 1. Native browser
- [ ] Custom Sk/Ai application window and native Windows lifecycle.
- [ ] Chromium engine embedded through CEF; standard public web PKI/TLS only. Russian GOST certificates and CryptoPro are explicitly out of scope.
- [x] Initial native CEF tab controller: create, close, switch, loading/title/address synchronization. (Restore/session recovery and real favicons remain open.)
- [ ] Address bar accepts URLs and search queries.
- [ ] Back, forward, reload, stop and home.
- [ ] Bookmarks with folders and editing.
- [ ] Browsing history with search and clearing controls.
- [ ] Downloads with progress, open, show-in-folder and remove.
- [ ] Extension manager and a documented compatibility policy.
- [ ] Multiple profiles and isolated profile data.
- [ ] Import/export bookmarks and settings.
- [ ] Find-in-page, print, zoom, fullscreen and page source/developer tools where appropriate.

## 2. Search
- [ ] User-selectable default search provider: Yandex, Google, Bing, DuckDuckGo, Brave Search or Startpage.
- [ ] Provider switcher beside the start-page search field.
- [ ] Address-bar search uses the selected provider.
- [ ] Search provider choice is independent of the AI mode.

## 3. Sk/Ai Assistant
- [ ] Persistent right-side docked panel; never a full-screen modal over the current website.
- [ ] Collapse, restore, resize and detach into a separate floating window.
- [ ] Floating window supports minimize, maximize, restore, close and native Windows always-on-top.
- [ ] Chat history and multiple conversations.
- [ ] Context remains available when switching Local AI, Cloud AI and Internet Search modes.
- [ ] Explicit user action before sharing current-page content with the assistant.
- [ ] Support text/code and common document attachments in staged releases.
- [ ] Explain when a capability is unavailable; never display fake model/security status.

## 4. AI modes
The mode selector belongs directly beside the assistant prompt field.

### Local AI
- [ ] Qwen 3.5 2B GGUF Q4_K_M baseline model.
- [ ] llama.cpp runtime, CPU-only supported.
- [ ] Target low-memory configuration: 8 GB system RAM.
- [ ] No prompt or page content leaves the device in local mode.
- [ ] Model download, integrity verification, install/update and model status.

### Cloud AI
- [ ] Provider selection and API-key storage using Windows-protected credential storage.
- [ ] Before sending any prompt, attachment or page content, show exactly what will be transferred and obtain explicit consent.
- [ ] Clear cloud-mode indicator.
- [ ] No silent fallback from Local to Cloud.

### Internet Search
- [ ] A distinct assistant mode that searches the web rather than pretending the local model has live access.
- [ ] Display sources and openable result links.
- [ ] Keep search mode distinct from the browser's default search provider.
- [ ] Preserve the current chat context when changing modes.

## 5. Privacy and security
- [ ] No first-party product analytics SDK or telemetry.
- [ ] No external font/CDN requests from local UI resources.
- [ ] Block known advertising and tracking domains with maintainable filter lists.
- [ ] Third-party cookie restrictions configurable by the user.
- [ ] Do not break first-party sign-in/session cookies by default.
- [ ] Per-site permissions for camera, microphone, location, notifications and clipboard.
- [ ] Standard Chromium certificate validation and Safe Browsing/security protections remain enabled.
- [ ] Do not fake cookie consent or bypass website security controls.
- [ ] AI page inspection is supplementary and must never replace Chromium security checks.
- [ ] Clear controls for history, cookies, cache, downloads and site data.
- [ ] Cloud transfer requires explicit consent; local mode remains local.

## 6. Appearance and interaction
- [ ] Reference screenshot is the visual source of truth: Aurora Glass / Soft Aurora.
- [ ] Themes: Soft Aurora, Quiet Mist and Deep Space.
- [ ] Light/dark follows Windows by default, with manual override.
- [ ] Local mode uses mint/teal; Cloud mode uses violet; Internet Search gets a separate search identity.
- [ ] Responsive layout and Windows DPI scaling.
- [ ] Respect reduced-motion accessibility settings.
- [ ] Logo slash animation reveals the Ai mark.
- [ ] No unwanted blank borders or decorative UI that obscures content.

## 7. Windows integration and delivery
- [ ] Per-user installer; no mandatory administrator rights for ordinary installation.
- [ ] Start menu shortcut, file associations only with consent, clean uninstall.
- [ ] Update channel and signed release packages.
- [ ] Crash reporting is opt-in; no silent telemetry.
- [ ] Native window management and always-on-top behavior.
- [ ] Runtime packaging includes CEF subprocess, locales, resources and sandbox components.
- [ ] Automated Windows x64 build and smoke tests.

## 8. Engineering rules
- Separate UI, browser controller, CEF integration, profile/storage, privacy, AI orchestration and Windows services.
- Do not mix the legacy extension prototype with the native-shell runtime.
- Keep secrets out of source code and logs.
- Pin external runtime versions and verify downloaded artifacts by cryptographic hash.
- Mark each milestone as scaffolded, built, runtime-tested or released. Never conflate these states.

## Current milestone
Native C++/Win32 + CEF application scaffold, local Aurora UI and first native multi-tab controller. Not yet compiled or runtime-tested on Windows. Next: successful Windows CI build, then tab lifecycle hardening, browser profile/storage and CEF runtime packaging.
