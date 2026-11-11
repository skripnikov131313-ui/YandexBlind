# Yandex Blind

Yandex Blind is a Windows 11 x64 browser distribution built around Chromium-Gost, with a custom local start page and an embedded on-device Qwen assistant.

## Included in this build

- Chromium-Gost 156.0.8078.88 (x64), retaining its upstream ГОСТ TLS / msspi implementation.
- Custom Yandex Blind start page with configurable search provider (Yandex, Google, Bing, DuckDuckGo, Brave Search, Startpage).
- Local Qwen3.5-2B model in GGUF Q4_K_M format (about 1.31 GB).
- llama.cpp CPU inference runtime, launched silently and bound to 127.0.0.1 only.
- Integrated assistant panel with text-file context for TXT, MD, CSV, LOG and JSON files.
- Dark/light/system themes, reduced-motion option, and dedicated browser profile.
- Single Windows installer; no separate Ollama window or user-managed terminal.

## Hardware target
The default model is selected for basic Windows 11 laptops with 8 GB RAM and no discrete GPU. It runs on CPU. Performance depends heavily on CPU generation and available memory; keep other memory-heavy apps closed on 8 GB systems. The small model is suitable for everyday short text tasks, summaries, rewriting and basic document assistance. It is not a substitute for a larger model for complex reasoning or long documents.

The assistant currently accepts text files up to 300 KB and truncates the local context before sending it to the model. DOCX and PDF extraction are not yet implemented.

## Build
GitHub Actions downloads the upstream Chromium-Gost archive, verifies its SHA-256, downloads the official llama.cpp Windows CPU runtime, downloads the Qwen GGUF model and verifies the model SHA-256, then packages all components with Inno Setup.

- Chromium-Gost release: https://github.com/deemru/Chromium-Gost/releases/tag/156.0.8078.88
- llama.cpp runtime release: https://github.com/ggml-org/llama.cpp/releases/tag/b12248
- Qwen model: https://huggingface.co/gguf/Qwen3.5-2B-GGUF

## Privacy model

- The project adds no analytics SDK or telemetry endpoint.
- The local model server listens on 127.0.0.1 only and is stopped by the launcher when the browser closes.
- The AI panel sends prompt text and supported attached text files to the local inference server only. It does not send them to a cloud LLM.
- The browser launcher disables several Chromium background reporting/sync components. This is not a guarantee of zero network traffic or anonymity.
- Search providers and websites receive ordinary network requests, including the user's IP address and request metadata needed to serve pages.
- Certificate validation and browser security protections remain enabled. Do not install untrusted root certificates or bypass TLS validation.

## CryptoPro CSP and certificates

Chromium-Gost provides ГОСТ TLS through its upstream msspi integration. This project does not bundle, install, license, or silently modify CryptoPro CSP. CSP presence and compatibility still require validation on a real Windows 11 machine with the required CryptoPro version and certificate store.

## Third-party components

Chromium-Gost/Chromium, llama.cpp and Qwen are third-party components. Their upstream notices and licenses apply. Qwen3.5-2B-GGUF is published under Apache-2.0; see the model repository for full terms.

## Assistant mode switch

The assistant panel includes a persistent two-mode switch:
- **Local AI**: prompt and supported text-file context are sent to the local Qwen server at 127.0.0.1.
- **Internet search**: opens the selected search provider in a new browser tab with the typed query. The query and ordinary network metadata are disclosed to that provider. Attachments are not sent in this mode.

The current internet mode opens the provider's search-results page; it does not yet fetch pages, extract sources, or ask Qwen to synthesize a cited answer. That is a separate web-retrieval integration step.
