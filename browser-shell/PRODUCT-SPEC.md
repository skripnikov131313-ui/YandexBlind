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
- [~] Restore up to 50 recent HTTP(S) tabs in their saved order, with the previous active tab restored last. Crash-recovery semantics and Windows runtime validation remain open.
- [~] Local bookmarks: add/remove current page, editable titles, create/rename/delete folders, move between folders and filter. Persisted in the UI CEF profile; Windows runtime validation remains open.
- [~] Local browsing history: capture completed native CEF navigations, search, remove individual entries and clear all. Persistence and navigation capture still require Windows runtime validation.
- [~] Native CEF download handler saves to Downloads/SkAi, emits progress and exposes pause/resume/cancel/open/show actions. Download history persists in the CEF UI profile; unfinished downloads are marked interrupted after restart. Windows runtime validation remains open.
- [ ] Extension manager and documented compatibility policy.
- [~] Multiple named profiles with separate CEF request contexts/cache directories; UI state is namespaced per profile. Windows runtime isolation and profile-switch lifecycle still require validation.
- [~] Import/export of bookmarks uses a versioned JSON schema, validates HTTP(S) URLs, deduplicates imported entries and rejects files over 5 MiB. Export/import of the remaining settings is not implemented.
- [~] Find-in-page via CEF FindHandler, previous/next match navigation, print and zoom controls are wired to the active native browser. Windows runtime testing remains pending.
- [~] Keyboard shortcuts for address focus, tabs, navigation, history, downloads, bookmarks, find-in-page and zoom. Runtime validation remains pending.
- [ ] Fullscreen and developer tools.
- [~] CEF permission prompts for camera, microphone, screen/audio capture, geolocation, notifications, clipboard and other supported permission types. Explicit allow/deny, optional remember-per-origin-and-type, saved decision list and revoke. Windows runtime validation and integration with a full permissions/settings database remain open.

## 2. Search

- [~] Provider URL support for Yandex, Google, Bing, DuckDuckGo, Brave Search and Startpage.
- [~] Provider cycle control on the start page.
- [~] Address bar and start-page search use the selected provider.
- [~] Selected provider is stored independently from the AI mode.
- [~] Settings view exposes the provider picker, theme selection and reduced-motion control.

## 3. Sk/Ai Assistant

- [~] Right-side docked assistant panel in the browser UI.
- [~] Collapse and restore the docked panel.
- [~] Detach into a separate native CEF popup window.
- [~] Floating-window minimize, maximize, restore, close and native topmost behavior.
- [~] Redock the assistant into the browser panel.
- [~] Conversation and Local/Cloud mode persistence scaffolding.
- [~] Bookmarks, bookmark folders, history, remembered site permissions, recent download metadata and the last 50 HTTP(S) tabs use the local CEF UI profile. History is capped at 2,000 entries and downloads at 200 records.
- [~] Cross-window chat/mode synchronization via storage events and profile-scoped keys; same-origin behavior and persistence still require Windows CEF runtime validation.
- [ ] Multiple named conversations and search.
- [~] Explicit user-triggered page-text transfer from the active HTTP(S) tab into the docked or floating assistant, capped at 6,000 characters and kept in memory only. Web content is marked as untrusted material in the local model request; Windows runtime behavior remains to be tested.
- [ ] Text/code and common document attachments.
- [ ] Resizable docked panel and keyboard shortcuts.

## 4. AI modes

The mode selector belongs next to the assistant composer.

### Local AI
- [~] Qwen 3.5 2B GGUF Q4_K_M model path pinned to Hugging Face revision `19bb3a8fe3d2976d563e3c61e69c75e61806f2bc`, and SHA-256 verification are implemented in the native installer; Windows runtime validation is pending.
- [~] Pinned llama.cpp v0.9.0 CPU-only server build uses GGML dynamic backend loading and CPU variants from x64 baseline through SSE4.2, AVX and AVX2-capable profiles. Windows build and runtime selection validation are pending.
- [~] CPU-only runtime uses a bounded 4,096-token context, one inference slot and a capped CPU thread count. Performance on an 8 GB Windows device still needs measurement.
- [~] HTTPS model download to LocalAppData, incremental SHA-256 verification, atomic install, model status and local server health check are implemented in source. Resume, update and Windows validation remain open.
- [~] Local chat requests use an ephemeral API key and a loopback-only llama-server; no cloud fallback is implemented. Network-capture and packaged runtime tests remain required.

### Cloud AI
- [~] Cloud mode is a non-transmitting placeholder.
- [ ] Provider selection and API-key storage through Windows-protected credential storage.
- [ ] Explicit transfer preview and consent before sending any prompt, attachment or page context.
- [ ] No silent fallback from Local to Cloud.

### Internet Search
- [~] A distinct Internet Search mode opens the selected search provider in a new browser tab while preserving the assistant conversation in both docked and floating panels. In-panel source extraction and citations are not implemented.
- [ ] Preserve chat context when switching among Local, Cloud and Internet Search.

## 5. Privacy and security

- [~] No project analytics SDK or telemetry endpoint in the native-shell source.
- [~] Local UI assets are bundled; no font/CDN is required.
- [~] Native CEF request filtering blocks a curated list of known third-party ad, analytics, session-replay and fingerprinting endpoints on subresource requests; top-level navigation and downloads are preserved. List coverage and compatibility testing remain open. The URL matching logic has dedicated native C++ unit tests in Windows CI.
- [~] Chromium per-profile cookie controls are configured to block third-party cookies while retaining first-party cookies. Runtime verification and per-site exceptions remain open.
- [~] Known marketing/click-attribution IDs are stripped from HTTP(S) GET navigation URLs while functional parameters, fragments and POST navigations are preserved. First-party JavaScript fingerprinting and site `localStorage`/IndexedDB identifiers are not neutralized.
- [~] Per-origin permission decisions are stored in the local profile; the settings UI lists remembered allow/deny choices and supports revocation. Unremembered requests are denied unless the user explicitly allows them.
- [~] Standard Chromium certificate validation and sandbox are retained in the intended build configuration; this still requires Windows runtime verification.
- [ ] Verify Chromium Safe Browsing and security services in the packaged runtime.
- [~] History can be searched, individual entries removed and all history cleared. Download metadata persists, completed files reopen only from the managed Downloads/SkAi directory; cookie/cache/site-data controls remain open.
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
- [~] One-shot slash-to-Ai logo reveal is implemented in CSS; final motion polish and Windows runtime review remain open.
- [ ] Review responsive behavior at supported Windows window sizes.

## 7. Windows integration and delivery

- [~] CEF runtime packaging is configured: bootstrap EXE, client DLL, Chromium DLLs and resource/locale files.
- [~] Windows x64 GitHub Actions workflow includes build, runtime-file checks and native-window smoke test.
- [ ] Successful Windows x64 CI run and downloadable artifact.
- [ ] Real Windows 11 runtime smoke test across multiple tabs and popup lifecycle.
- [~] PowerShell per-user installer and uninstaller scripts, with Start menu shortcut and preservation of browser user data. Packaging and Windows execution validation remain pending.
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

Native C++/Win32 + CEF shell with tabs/navigation controls, session restoration, bookmark folders/editing, local history, persistent download history, native permission prompts and remembered per-site permission management is implemented in source. The current CEF build uses the post-CEF-138 bootstrap architecture and explicitly refuses to start without the sandbox handle.

JavaScript syntax and HTML-to-JS element-reference checks pass in the repository inspection. The Windows workflow has not yet produced a successful build artifact; its latest run is queued and no Windows compile/runtime result is claimed. No Windows runtime behavior is claimed as tested. Next gate: complete Windows CI build and native-window smoke test. After that: multiple profiles, extension compatibility, cookie/cache/site-data controls and broader browser settings, then integrate local AI.
