# Sk/Ai Native Browser Architecture

## 1. Product boundary

Sk/Ai is the browser application and product. Upstream Chromium is the internal web engine and foundation for compatibility and security.

We fork and modify the Chromium browser UI/application layer. We do not implement Blink, V8, HTML/CSS layout, JavaScript execution or the network stack from scratch.

The product has no GOST TLS, msspi or CryptoPro CSP requirement. It uses ordinary Chromium TLS and platform certificate stores.

## 2. Main window composition

    SkAiBrowserWindow
    ├── NativeWindowFrame
    │   ├── SkAiWindowCaption
    │   └── WindowControls (minimize / maximize / close)
    ├── SkAiTabStrip
    │   ├── BrowserTab*
    │   └── NewTabButton
    ├── SkAiNavigationToolbar
    │   ├── Back / Forward / Reload
    │   ├── SiteSecurityIndicator
    │   ├── SkAiOmnibox
    │   ├── BookmarkButton
    │   ├── DownloadsButton
    │   └── Profile / MainMenu
    ├── SkAiWorkspace
    │   ├── SkAiNavigationRail
    │   ├── WebContentsRegion
    │   └── SkAiAssistantDock (optional, resizable)
    └── BrowserServices
        ├── Chromium Profile / TabStripModel / WebContents
        ├── Permission and Privacy Services
        ├── Download Manager / Session Restore
        └── Local AI Bridge

Existing Browser, Profile, TabStripModel, WebContents, LocationBarModel, navigation controller, permission manager, download manager and security-state services remain canonical. Sk/Ai controls call these services; they do not create parallel tab or network implementations.

## 3. Split workspace

- Assistant is part of the native browser layout, not a fixed-position overlay.
- Web content is resized to the exact remaining width.
- Divider is draggable and keyboard accessible.
- Minimum content width prevents the webpage from collapsing.
- Dock can be hidden and restored without destroying chat state.
- Left/right placement is a persisted preference.
- Floating mode is a separate native browser-owned window.
- Always-on-Top is an optional Windows behavior, off by default and visibly indicated when active.

## 4. Tabs and navigation

Native Chromium state remains canonical. Sk/Ai replaces presentation and user-facing command routing, not:
- tab lifecycle, drag/drop, tear-off and session restore;
- site isolation and process model;
- omnibox navigation and autocomplete;
- downloads and permission models;
- certificate indicators and security interstitials.

Omnibox input uses Chromium's navigation/search pipeline. Search provider is a user preference independent of AI mode. Yandex may be the default; Google, Bing, DuckDuckGo, Brave Search and Startpage remain selectable.

## 5. Soft Aurora native design tokens

The same palette must be shared across native Views, the start page and assistant UI.

### Dark
- Window: #171923
- Elevated surface: #222533
- Higher surface: #2B2F40
- Text: #F3F1FA
- Muted text: #B0AFC2
- Lavender: #C6B6FF
- Mint / Local AI: #9BD9C6
- Rose: #E6AECA
- Sky: #91C4E8

### Light
- Window: #F3F2F7
- Elevated surface: #FFFFFF
- Text: #28283A
- Muted text: #74748A
- Lavender: #7768B6
- Mint / Local AI: #438F7C
- Rose: #B8759B
- Sky: #568EAE

Local AI remains mint. Cloud AI remains lavender. Use Chromium ColorIds / ColorProvider mixers for native UI colors; direct SkColor values are reserved for transient effects. Respect operating-system theme by default with a manual override.

## 6. Privacy and AI

- The local inference process binds only to 127.0.0.1.
- Prompts, supported page snippets and text files are sent only to local inference in Local mode.
- Page context is collected only after explicit user action.
- Password inputs, payment fields, cookies, hidden form values, browser history and profile data are never added to prompt context by default.
- Page content is untrusted data and cannot override assistant system instructions.
- Cloud mode remains disabled until provider selection, secure secret storage, data disclosure and per-request consent exist.
- Switching mode never uploads the current conversation automatically.
- No project analytics SDK or project telemetry endpoint is added.

## 7. Browser security baseline

- Use upstream Chromium TLS and certificate validation with Windows platform certificate stores.
- No GOST TLS, msspi or CryptoPro CSP integration is in scope.
- Preserve sandboxing, site isolation, Safe Browsing and certificate error interstitials.
- Never disable certificate validation to work around a site or test failure.
- Microphone, camera, geolocation, notifications and clipboard require explicit permission.
- Privacy controls must not spoof cookie consent or silently break first-party website state.

## 8. Process and memory model

- No browser-wide AI process is started until user enables AI or opens the assistant.
- Qwen CPU inference is throttled and memory-bounded on the 8 GB runtime target.
- The 2B model is optional at install time.
- Inactive tabs may use Chromium's native lifecycle/discard mechanisms.
- UI remains responsive while inference runs.
- Model failures do not block navigation or crash browser processes.

## 9. Delivery gates

1. Source gate: pinned upstream Chromium source checkout and dependencies complete.
2. Baseline gate: pristine upstream Chromium compiles on the large Windows build host.
3. UI gate: native Soft Aurora, brand and toolbar patches compile and open.
4. Navigation gate: tabs, omnibox, restore, downloads and permissions work.
5. Assistant gate: docked assistant resizes content and supports floating mode.
6. Security gate: standard TLS, Windows certificate stores, certificate errors, sandboxing and Safe Browsing regressions pass.
7. Performance gate: startup, memory and tab lifecycle are measured on 8 GB Windows machines.
8. Release gate: reproducible per-user Windows installer and update channel.
