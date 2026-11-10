# Sk/Ai

Sk/Ai is a Windows x64 browser distribution built around Chromium-Gost, with a custom start page and an optional on-device Qwen assistant.

## Project status

This repository currently packages upstream Chromium-Gost and a Manifest V3 start-page extension. It is **not yet a fully custom Chromium browser shell**: the native tab strip, omnibox, toolbar and browser chrome remain upstream Chromium UI.

The Local / Cloud control is present in the assistant interface. Local mode is implemented against the bundled loopback inference server. Cloud mode is currently a safe placeholder: no cloud provider is connected and selecting it sends nothing.

## Current build contents

- Chromium-Gost 156.0.8078.88 (Windows x64), retaining upstream ГОСТ TLS / msspi support.
- Sk/Ai start page with configurable search provider: Yandex, Google, Bing, DuckDuckGo, Brave Search or Startpage.
- Local Qwen3.5-2B GGUF Q4_K_M model.
- llama.cpp CPU inference runtime, bound to 127.0.0.1.
- Assistant panel with text-file context for TXT, MD, CSV, LOG and JSON files (up to 300 KB).
- Soft Aurora visual system, light/dark/system theme selection, Quiet Mist and Deep Space atmospheres, and reduced-motion option.
- Dedicated browser profile and per-user installer.

## Hardware target

The default model targets Windows 11 x64 systems with 8 GB RAM and no discrete GPU. It runs on CPU. Performance depends on CPU generation and available memory; a 2B model is a compromise for short everyday text tasks and is not a substitute for a larger model for complex reasoning or long documents.

The assistant currently truncates attached text context before sending it to the local model. DOCX and PDF text extraction are not implemented.

## Privacy model

- The project does not add an analytics SDK or a project telemetry endpoint.
- The local model server listens on 127.0.0.1 only and is started by the launcher when model files are present.
- In Local mode, prompts and supported attached text are sent to the local inference server only.
- The current conversation is held in the start-page JavaScript session and is lost when that page is reloaded or closed. Persistent chat history is not implemented yet.
- Cloud mode is not connected. Selecting it displays a notice; no prompt or conversation is transmitted.
- The launcher disables selected Chromium background reporting/sync components. This is not a guarantee of zero network traffic or anonymity.
- Websites and the selected search provider receive ordinary network requests, including the user's IP address and request metadata needed to serve pages.
- Certificate validation and Chromium security protections remain enabled.

## GOST and CryptoPro CSP

Chromium-Gost provides ГОСТ TLS through its upstream implementation. This project does not bundle, install, license or silently modify CryptoPro CSP. CSP presence, certificate-store behavior and compatibility must be validated on a real Windows 11 system with the required CryptoPro version and certificates.

## Build

GitHub Actions uses a Windows runner to download and verify the Chromium-Gost archive and Qwen model, download the llama.cpp CPU runtime, and package a per-user installer with Inno Setup.

The workflow can be run manually or by pushing changes to the configured development and main branches.

## Third-party components

Chromium-Gost/Chromium, llama.cpp and Qwen are third-party components. Their upstream notices and licenses apply. Verify the model repository's current license and notices before redistributing a release.
