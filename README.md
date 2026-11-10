# Sk/Ai

Sk/Ai is a Windows x64 browser distribution built around Chromium-Gost, with a custom start page and a native Chromium side panel for its on-device Qwen assistant.

## Product direction

The intended layout is a split workspace: the current website or search page remains in the main browser area on the left; Sk/Ai Assistant is docked in the browser side panel on the right. It must not appear as a modal overlay over website content. Users can choose the side-panel position in Chromium's browser settings.

The current repository is still a Chromium-Gost distribution plus an extension. The native tab strip, address bar, toolbar and window frame remain upstream Chromium UI; a fully custom Sk/Ai browser shell is a separate future engineering phase.

## Current build contents

- Chromium-Gost 156.0.8078.88 (Windows x64), retaining upstream ГОСТ TLS / msspi support.
- Sk/Ai start page with configurable search provider: Yandex, Google, Bing, DuckDuckGo, Brave Search or Startpage.
- Native Chromium Side Panel extension page for Sk/Ai Assistant, opened from the browser action or Alt+Shift+S.
- Local Qwen3.5-2B GGUF Q4_K_M model with llama.cpp CPU inference runtime, bound to 127.0.0.1.
- On-demand current-page text transfer using activeTab and scripting, rather than permanent access to all sites. The user explicitly requests page reading from the panel; the toolbar action / shortcut is the permission-granting invocation.
- Local file context for TXT, MD, CSV, LOG and JSON files (up to 300 KB).
- Local assistant conversation persistence in the browser extension profile, with a New Chat control.
- Soft Aurora themes, system/light/dark selection, Quiet Mist and Deep Space atmospheres, and reduced-motion option.

## Hardware target

The default model targets Windows 11 x64 systems with 8 GB RAM and no discrete GPU. It runs on CPU. Performance depends on CPU generation and available memory; a 2B model is a compromise for short everyday text tasks and is not a substitute for a larger model for complex reasoning or long documents.

The model is configured with a 2048-token context. Page and file content is truncated before inference to keep resource use bounded. DOCX and PDF text extraction are not implemented.

## Assistant behavior

- **Local** mode sends prompts and supported text context only to the bundled inference server at 127.0.0.1.
- The default side panel is native browser UI docked alongside the current website; it is not a floating modal over the page.
- The assistant can also detach into a separate popup window and be re-docked. In floating mode it can be moved, minimized, maximized and restored using normal window controls.
- Always-on-top across all Windows applications is not implemented by the extension; it would require a native Windows companion.
- The docked panel can remain available while switching tabs, according to Chromium's side-panel behavior.
- Page content is read only after the user presses the explicit "Передать страницу" action. The extension uses temporary activeTab access instead of permanently reading every site.
- Conversation history is stored locally in the browser profile and can be cleared with "Новый диалог".
- **Cloud** mode is a safe placeholder. No cloud provider is connected; switching to it does not transmit prompts, page text, files or chat history.
- Context does not disappear when switching Local / Cloud. A future cloud provider must request informed consent and show exactly which messages and attachments will leave the device.

## Privacy model

- The project does not add an analytics SDK or a project telemetry endpoint.
- The local model server listens on 127.0.0.1 only and is started by the launcher when model files are present.
- The assistant does not send prompts or supported attached text to a cloud LLM in Local mode.
- Persisted chat data is local to the browser profile; it is not currently encrypted by an application-specific layer. Users can clear the current conversation inside the panel or clear extension/browser data.
- The launcher disables selected Chromium background reporting/sync components. This is not a guarantee of zero network traffic or anonymity.
- Websites and the selected search provider receive ordinary network requests, including the user's IP address and request metadata needed to serve pages.
- Certificate validation and Chromium security protections remain enabled.

## Permissions

The extension requests sidePanel, scripting, activeTab, storage, and its existing privacy/filtering permissions. It does not request permanent access to every website. Page text is retrieved only after a user action and only when Chromium has granted access to the active tab. Browser-internal pages and protected pages may not permit script injection.

## GOST and CryptoPro CSP

Chromium-Gost provides ГОСТ TLS through its upstream implementation. This project does not bundle, install, license or silently modify CryptoPro CSP. CSP presence, certificate-store behavior and compatibility must be validated on a real Windows 11 system with the required CryptoPro version and certificates.

## Build and verification

GitHub Actions uses a Windows runner to download and verify the Chromium-Gost archive and Qwen model, download the llama.cpp CPU runtime, and package a per-user installer with Inno Setup.

The installer has not yet been verified on a real Windows 11 machine. The side-panel integration has been committed but still requires a successful Windows CI build and runtime test against this Chromium-Gost distribution.

## Third-party components

Chromium-Gost/Chromium, llama.cpp and Qwen are third-party components. Their upstream notices and licenses apply. Verify the model repository's current license and notices before redistributing a release.


## Native Sk/Ai browser shell

The native browser is being developed separately from the start-page extension prototype. It will replace Chromium's stock browser chrome with a Sk/Ai Windows interface while retaining the Chromium-Gost rendering and GOST/msspi engine.

See [native-shell/README.md](native-shell/README.md) for the pinned source baseline, architecture, Windows build requirements and source-build scripts. The first source milestone is a reproducible Chromium-Gost checkout and baseline build; native UI patches are a separate, compile-tested series.
