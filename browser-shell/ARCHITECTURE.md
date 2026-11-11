# Sk/Ai Native Shell — Architecture and Build

## Product architecture

- Product target: standalone Windows 11 x64 browser with a custom C++20 Win32 shell.
- Rendering engine: Chromium embedded through CEF. GOST TLS, Russian root-certificate overrides and CryptoPro are out of scope.
- Native UI: Aurora Glass interface from `ui/`, rendered by a dedicated CEF browser.
- Web contents: separate CEF browser instances per native tab. Native controller owns tab IDs, active state, bounds and lifetime.
- Assistant: docked in the main UI; detach creates a separate native CEF popup with Windows topmost behavior. The popup can be closed to redock.
- Local AI: native C++ module downloads Qwen 3.5 2B Q4_K_M GGUF from immutable Hugging Face revision `19bb3a8fe3d2976d563e3c61e69c75e61806f2bc` to `%LOCALAPPDATA%/SkAi/Models`, verifies the pinned SHA-256, and starts a separately packaged llama.cpp v0.9.0 CPU server on `127.0.0.1` with an ephemeral API key. The docked and floating assistant send OpenAI-compatible chat requests through the CEF-to-native bridge. Windows CI build, native-window smoke test and package assembly have passed; real Windows 11 inference and 8 GB device performance tests remain pending.
- Cloud AI: placeholder only; no provider is connected and no prompts/page content are sent.
- Internet Search: a third assistant mode opens the selected Yandex, Google, Bing, DuckDuckGo, Brave Search or Startpage results page in a new native tab. The conversation remains in the assistant; search result extraction and in-panel source citations are not part of this first implementation.
- Persistence: browser profile UI data is located under `%LOCALAPPDATA%/SkAi/Profiles/default`. The trusted Sk/Ai UI uses a persistent CEF context with a non-empty `CefRequestContextSettings.cache_path`; bookmarks, bookmark folders, history, download metadata, last-open HTTP(S) tabs and assistant conversations are stored in profile-scoped UI `localStorage`. Ordinary web tabs use a separate per-profile CEF request context with an empty `cache_path` (off-the-record/in-memory mode), so site cookies, localStorage, IndexedDB and service-worker storage are not persisted across browser restarts. All real site cookies are denied by the CEF cookie access filter on send and save. The renderer exposes a per-session in-memory `document.cookie` facade for client-side compatibility; no site cookie is sent to the server. Server-side authentication that requires a real cookie may fail. CEF 154 no longer exposes the older `persist_user_preferences` flag on `CefSettings` or `CefRequestContextSettings`. Remembered per-origin permission decisions are stored in `permissions.json` in the same profile and atomically replaced on updates. Cross-session behavior still requires Windows runtime validation.

## CEF version and sandbox

The build pins CEF `154.0.34+g14c5a08+chromium-154.0.8037.98`. The legacy `CefRegisterExtension` API is removed in CEF 154, so the native bridge is installed through `CefRenderProcessHandler::OnContextCreated` and `CefV8Value::CreateFunction`. The bridge is exposed only in the main frames of the packaged `ui/index.html` and `ui/assistant.html` pages; ordinary web pages and their subframes do not receive native functions.

CEF 138+ Windows client builds use the bootstrap architecture. Sk/Ai is built as `SkAi.dll`; the matching `bootstrap.exe` is copied as `SkAi.exe`. The bootstrap calls the exported `RunWinMain` entry point and supplies the sandbox handle. The client passes that handle to `CefExecuteProcess` and `CefInitialize` and rejects startup if the handle is missing. The build does not silently disable the Chromium sandbox.

## Build locally on Windows

Requirements:
- Windows 11 x64.
- Visual Studio 2022 with the C++ desktop workload and a Windows SDK supported by the pinned CEF package.
- CMake 3.24 or newer.
- Matching CEF 154 Windows x64 binary distribution extracted locally.

PowerShell:

```powershell
$env:CEF_ROOT = "C:\path\to\cef_binary_154.0.34+g14c5a08+chromium-154.0.8037.98_windows64"

cmake -S browser-shell -B out/skai -A x64 -DCEF_ROOT="$env:CEF_ROOT" -DUSE_SANDBOX=ON
cmake --build out/skai --config Release --parallel 2
```

The packaged output is written to `out/skai/bin/Release`. It contains `SkAi.exe` (CEF bootstrap), `SkAi.dll` (Sk/Ai client), CEF runtime DLLs, resource packs, `locales/` and the local `ui/` directory. Downloads are written to the user Downloads folder under `SkAi`; only paths resolving inside that managed directory can be opened from persisted download history. CEF permission requests are mediated by the native permission handler; decisions are explicit, optional to remember per origin and permission mask, and revocable from Settings.

## Site tracking protection

The application UI has no analytics SDK, session recorder or app-generated tracking identifier. The host/path matching logic and URL cleanup helper are covered by standalone C++ tests in the Windows CI workflow. For visited websites, a native `CefRequestHandler` cancels requests to a curated set of known ad, analytics, session-replay, fingerprinting and remote consent-management hosts. It does not block top-level navigations or downloads. For HTTP(S) GET navigations, known marketing/click-attribution query keys (UTM fields, `gclid`, `fbclid`, `msclkid`, `ttclid`, `yclid`, and related IDs) are removed before navigation; functional query keys, fragments and POST/form navigations are preserved. Each persistent Chromium profile sets `profile.cookie_controls_mode=1` (Chromium `kBlockThirdParty`) from `CefRequestContextHandler::OnRequestContextInitialized`, after preferences are registered, so third-party cookies are blocked while first-party sign-in cookies remain available. A CEF cookie access filter additionally rejects known advertising/analytics cookie names on both save and send, including main-frame navigations; ordinary first-party session cookies remain available for site functionality. A context-level request-filter fallback covers Service Worker and `CefURLRequest` traffic that has no associated browser/frame. The privacy UI explicitly notes that this is best-effort tracking reduction, not complete protection against first-party JavaScript fingerprinting; site `localStorage` and IndexedDB are not erased. A renderer-side compatibility shim is installed only in HTTP(S) page contexts, never in the trusted Sk/Ai UI. It provides a local GPT-compatible slot lifecycle that reports `isEmpty: true`, no-op AdSense and analytics entry points, common ad-creative hiding, an IAB TCF response with all represented purposes denied, Global Privacy Control and Do Not Track signals. It does not claim an ad impression, send a click/impression beacon, grant consent, or transmit analytics. All `navigator.sendBeacon` calls are locally suppressed, and common first-party telemetry paths in fetch/XHR receive local empty success responses. Arbitrary first-party API calls remain a classification gap. This is compatibility scaffolding, not proof that every ad-gated site will unlock its content. First-party cookies and site storage still require a dedicated isolation/clearing policy and are not yet fully neutralized.

## Local AI runtime

The Windows workflow also builds `llama-server.exe` from llama.cpp v0.9.0 (verified against commit `cd959bae99ff4bed22bef2604f45241c09bbb8c8`) with CPU-only settings, dynamic CPU backend loading and runtime-dispatched x64/SSE4.2/AVX/AVX2-family variants. The packaged executable directory includes the matching `ggml-cpu-*.dll` backends under `runtime/llama/`; llama.cpp selects the best supported variant on the current CPU. The GGUF installer downloads the pinned Q4_K_M file from `gguf/Qwen3.5-2B-GGUF` over HTTPS, computes SHA-256 incrementally with Windows CNG/BCrypt, and only atomically installs the file after the published digest matches. The original Qwen3.5-2B model is published under Apache-2.0; model and conversion attribution must be retained in the packaged distribution. Local inference binds to `127.0.0.1`, uses an ephemeral API key, disables the server Web UI, and does not use a network proxy for inference requests. The Cloud mode is still a non-transmitting placeholder.

## CI

`.github/workflows/native-shell-windows.yml` downloads and verifies the pinned CEF archive, runs JavaScript syntax and UI regression checks, builds the native shell, builds the pinned CPU llama.cpp server, packages its required runtime files, starts the server with `--version` as a dependency smoke test, checks required runtime files, starts Sk/Ai for a native-window smoke test, then uploads `skai-win-x64-runtime`.

Windows CI run #428 completed all 14 steps successfully, including C++ build, tracking tests, llama.cpp runtime build, bundled-model verification, native-window smoke test and ZIP artifact upload. This is CI validation only: a real Windows 11 installation, end-to-end inference quality and performance on an 8 GB device still require hands-on testing.

## Product boundaries

The existing extension-based branch is retained as a separate prototype/reference. It is not the runtime or UI shell for this native CEF branch.


## Per-user installation

The release package includes PowerShell install/uninstall scripts. Installation is scoped to the current Windows user under `%LOCALAPPDATA%/Programs/SkAi`; no administrator elevation is requested. The installer adds a Start menu shortcut and a per-user Installed Apps record. Uninstall removes only program files and shortcuts and deliberately preserves browser data and downloaded models under `%LOCALAPPDATA%/SkAi`. The Windows CI assembles a ZIP package after the runtime smoke test.


## AI-first product rule

Sk/Ai ships as one integrated browser application. The assistant UI, native bridge, inference controller, model manager and security-analysis orchestration are built into the native package; they are not a browser extension or a separately installed companion app. The current model weights are a separately provisioned data asset because the GGUF is approximately 1.4 GB. Provisioning must be automatic on first launch, with visible progress and retry/cancel controls; the user must not have to discover a separate "install AI" feature. The browser shell and deterministic security protections work during provisioning. Local inference is the default, and no silent cloud fallback is permitted.

## Integrated security architecture

The security system is deliberately split between enforcement and explanation:

- **Native policy engine — authoritative.** It evaluates URL/origin and redirect chains, CEF resource requests, permission prompts, download metadata and browser isolation rules. Its decisions are deterministic and do not depend on model availability or model output. Critical blocks happen synchronously at the browser boundary where possible.
- **Local AI security analyst — advisory.** It receives only bounded structured evidence and optional user-selected page text. It can identify phishing language, impersonation, suspicious forms, social engineering, risky permission requests and download context; explain why a deterministic rule fired; and summarize a site's risk. It cannot grant permissions, allow a request, change protection settings, execute code, access secrets or call privileged native functions.
- **Security Center — user-facing control plane.** It presents protection status, per-site analysis, blocked requests, redirect and download warnings, permission decisions, and evidence-based explanations. It remains available when local inference is not ready.

All web-derived content is tainted as untrusted data. Prompt instructions embedded in page text, DOM, metadata, filenames or documents are quoted evidence, never authority. Model output is treated as untrusted too: the native layer validates any proposed action against a strict allowlist and still requires a separate user gesture. There is no model-generated direct execution path.

The AI analysis request schema should be narrow and versioned. It may include canonical URL/origin, redirect chain, Chromium certificate-validation outcome, permission request type, download source/name/MIME/size, and a bounded excerpt selected for analysis. It must not include all tabs, full history, cookies, credentials, local files or profile databases by default. Analysis stays on the loopback local runtime. Cloud analysis remains disabled until a provider is implemented with a per-transfer data preview and explicit consent.

The first implementation milestone is to build a native security controller and a versioned security-event model, connect deterministic CEF enforcement events to it, and expose read-only structured evidence to the local AI. Do not wire AI output directly into CEF request handlers or permission decisions.


## Bundled model and protection preference

The release package carries the pinned Qwen3.5-2B Q4_K_M GGUF file in `runtime/llama/`. CI fetches the immutable upstream revision and verifies the actual 1,312,164,352-byte size and pinned SHA-256 before packaging. `GetModelPath()` prefers this bundled asset, then falls back to the existing per-user model directory for compatibility. The UI queries model status at startup, not only when Settings is opened.

The Settings → Protection preference `skai.security.ai-analysis.v1` defaults to enabled. It gates user-requested page-context transfer into local inference and clears pending/staged page context when disabled. It is deliberately separate from native network enforcement: turning off AI analysis must not change tracking-request blocking, cookie handling, permission mediation or download policy. This is currently an on-demand page-analysis toggle, not a claim that continuous AI threat monitoring has shipped.


## Native security event journal (first implementation milestone)

The native security event journal is process-local and bounded to the latest 200 records. It stores an event ID, timestamp, event category, severity, origin, short summary and bounded evidence. It deliberately does not store full URLs, query strings, cookies, credentials, page text, local file contents or profile data. Current CEF event sources include known tracker blocks, main-frame redirects, permission requests and download start/completion/cancellation.

The journal is a foundation for the Security Center, not yet the complete user-facing feature. It is not persisted across restarts, does not yet stream events into the browser UI, and does not yet submit event evidence to Qwen. The next implementation milestone is a read-only native-to-UI event feed with filtering/clearing controls, followed by a consent-gated local AI explanation pipeline. Deterministic CEF enforcement remains independent of AI availability and output.


## Security Center UI event feed

The trusted browser chrome can request the native process-local security journal through the CEF V8 bridge. The Security Center renders event type, severity, timestamp, source origin and bounded evidence; it supports filtering, refresh and clearing the in-memory journal. The feed is read-only and does not expose page content, cookies, full URLs or local file contents. Events currently include tracker blocks, main-frame redirects, permission requests and user permission decisions, and download lifecycle transitions. AI interpretation of the feed is not yet wired; this interface currently displays deterministic native event records only.


## Security Center — local AI event analysis

The Security Center can submit an explicit, bounded snapshot of at most 30 recent native journal records to the local Qwen runtime. The snapshot contains only event type, severity, origin, short summary, bounded evidence and timestamp; it excludes full URLs, query strings, cookies, page text, credentials, local paths and download contents. The UI requires AI security analysis to be enabled and the local model to be ready. The request uses a dedicated system instruction that marks every event field as untrusted data and prohibits policy changes or privileged actions. Results are labelled advisory. Native CEF enforcement remains deterministic and never waits for or consumes model output. No cloud fallback exists. This is user-triggered analysis, not continuous inference.


### Deterministic navigation risk signals

Before the browser follows a main-frame GET navigation, the native client checks a small set of high-signal URL properties: credentials embedded in the URL, an HTTPS-to-HTTP redirect downgrade, and an IDN hostname represented with punycode. These conditions create advisory journal entries only; they do not automatically block navigation. Ordinary cross-domain redirects are not treated as suspicious by themselves. The check does not resolve domains, infer brand impersonation, or claim that a destination is malicious.
