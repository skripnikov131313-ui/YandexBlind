# Sk/Ai — Product Specification

This is the project-wide acceptance checklist. A control is not complete because it is visible: it must be implemented, built, tested and documented.

Status key:
- [ ] Not implemented.
- [~] Implemented in source / prototype; Windows build or runtime validation is still pending.
- [x] Built and tested against the stated acceptance criteria.

## Product goal

A standalone Windows 11 x64 browser with a C++20 Win32 shell and standard Chromium engine embedded through CEF. The product is a browser for everyday use with a custom Sk/Ai interface, integrated assistant workspace and explicit privacy controls. It is not a new browser engine.

GOST TLS, Russian root certificate overrides and CryptoPro CSP are out of scope.

## 1. Native browser

- [~] Custom borderless Sk/Ai application window and native Windows window controls.
- [~] CEF bootstrap architecture with the Chromium sandbox enabled in source.
- [~] Native CEF tab controller: create, close, activate, title/URL/loading/favicon synchronization.
- [~] Address bar accepts URLs and search queries.
- [~] Back, forward, reload and stop controls.
- [ ] Session restore and crash recovery.
- [ ] Bookmarks with folders, editing and persistence.
- [~] Local browsing history: capture completed native CEF navigations, search, remove individual entries and clear all. Persistence and navigation capture still require Windows runtime validation.
- [~] Native CEF download handler saves to Downloads/SkAi, emits progress and exposes pause/resume/cancel/open/show actions. The list is not yet restored after browser restart.
- [~] Native CEF downloads: progress, pause/resume/cancel, open and show in folder. The live list is session-only; persistent download history and Windows runtime validation remain open.
- [ ] Extension manager and documented compatibility policy.
- [ ] Multiple profiles and isolated profile data.
- [ ] Import/export of bookmarks and settings.
- [ ] Find-in-page, print, zoom, fullscreen and developer tools.
- [ ] Site permission manager for camera, microphone, location, notifications and clipboard.

## 2. Search

- [~] Provider URL support for Yandex, Google, Bing, DuckDuckGo, Brave Search and Startpage.
- [~] Provider cycle control on the start page.
- [~] Address bar and start-page search use the selected provider.
- [~] Selected provider is stored independently from the AI mode.
- [ ] Dedicated provider picker/settings page.

## 3. Sk/Ai Assistant

- [~] Right-side docked assistant panel in the browser UI.
- [~] Collapse and restore the docked panel.
- [~] Detach into a separate native CEF popup window.
- [~] Floating-window minimize, maximize, restore, close and native topmost behavior.
- [~] Redock the assistant into the browser panel.
- [~] Conversation and Local/Cloud mode persistence scaffolding.
- [~] Local bookmarks/history use the CEF UI profile storage; data is capped at 2,000 recent history entries.
- [ ] Verified cross-window and cross-session chat synchronization.
- [ ] Multiple named conversations and search.
- [ ] Explicit page-content transfer after a user action.
- [ ] Text/code and common document attachments.
- [ ] Resizable docked panel and keyboard shortcuts.

## 4. AI modes

The mode selector belongs next to the assistant composer.

### Local AI
- [ ] Qwen 3.5 2B GGUF Q4_K_M model integration.
- [ ] llama.cpp runtime, CPU-only.
- [ ] 8 GB RAM performance profile and bounded context.
- [ ] Model download, SHA-256 verification, install/update and health status.
- [ ] Guarantee and test that prompts/page content remain local.

### Cloud AI
- [~] Cloud mode is a non-transmitting placeholder.
- [ ] Provider selection and API-key storage through Windows-protected credential storage.
- [ ] Explicit transfer preview and consent before sending any prompt, attachment or page context.
- [ ] No silent fallback from Local to Cloud.

### Internet Search
- [ ] A distinct assistant mode that searches the web and returns source links.
- [ ] Preserve chat context when switching among Local, Cloud and Internet Search.

## 5. Privacy and security

- [~] No project analytics SDK or telemetry endpoint in the native-shell source.
- [~] Local UI assets are bundled; no font/CDN is required.
- [ ] Maintainable ad/tracker filter lists and network request filtering.
- [ ] Configurable third-party cookie restrictions while preserving first-party sessions.
- [ ] Per-site permission manager.
- [~] Standard Chromium certificate validation and sandbox are retained in the intended build configuration; this still requires Windows runtime verification.
- [ ] Verify Chromium Safe Browsing and security services in the packaged runtime.
- [ ] Clear controls for history, cookies, cache, downloads and site data.
- [~] Cloud placeholder sends nothing; production Cloud must require explicit consent.
- [ ] Security review and network-traffic audit.

## 6. Appearance and interaction

- [~] Aurora Glass / Soft Aurora desktop layout: tabs, toolbar, left rail, start page and right assistant.
- [~] Graphite/navy surfaces with violet, blue, cyan and mint accents.
- [~] Local mint and Cloud violet mode identities.
- [~] Windows system light/dark preference with manual override.
- [ ] Quiet Mist and Deep Space fully implemented as selectable themes.
- [~] Native DPI-aware main window and content bounds; real multi-monitor tests pending.
- [ ] Reduced-motion and full keyboard/accessibility audit.
- [ ] Final logo slash animation and motion polish.
- [ ] Review responsive behavior at supported Windows window sizes.

## 7. Windows integration and delivery

- [~] CEF runtime packaging is configured: bootstrap EXE, client DLL, Chromium DLLs and resource/locale files.
- [~] Windows x64 GitHub Actions workflow includes build, runtime-file checks and native-window smoke test.
- [ ] Successful Windows x64 CI run and downloadable artifact.
- [ ] Real Windows 11 runtime smoke test across multiple tabs and popup lifecycle.
- [ ] Per-user installer without mandatory elevation.
- [ ] Start menu shortcut, optional file associations and clean uninstall.
- [ ] Update channel and signed packages.
- [ ] Opt-in crash diagnostics; no silent telemetry.

## 8. Engineering rules

- Separate UI, browser controller, CEF integration, profile/storage, privacy, AI orchestration and Windows services.
- Keep the legacy extension prototype separate from the native-shell runtime.
- Keep secrets out of source and logs.
- Pin external runtime versions and verify downloaded archives.
- Report each milestone as source-implemented, built, runtime-tested or released. Never conflate these states.

## Current milestone

Native C++/Win32 + CEF shell with tabs/navigation controls, an independent assistant popup, local bookmarks/history screens, and a native CEF downloads manager is implemented in source. The current CEF build uses the post-CEF-138 bootstrap architecture and explicitly refuses to start without the sandbox handle.

JavaScript syntax and HTML-to-JS element-reference checks pass in the repository inspection. The Windows workflow has not yet produced a successful build artifact; its latest run is queued and no Windows compile/runtime result is claimed. No Windows runtime behavior is claimed as tested. Next gate: complete Windows CI build and native-window smoke test. After that: harden profile/session storage, implement core browser services (bookmarks/history/downloads/permissions), then integrate local AI.
