# Sk/Ai native architecture

## 1. Product boundary

Sk/Ai owns the browser chrome and product behavior. Chromium-Gost owns web rendering and the mature browser platform.

We do not fork Blink, V8, the network stack or certificate verification. We fork the browser application/UI layer and retain the upstream GOST patch series intact.

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
    │   └── MainMenuButton
    ├── SkAiWorkspace
    │   ├── SkAiNavigationRail
    │   ├── WebContentsRegion
    │   └── SkAiAssistantDock (optional, resizable)
    └── BrowserServices
        ├── Chromium Profile / TabStripModel / WebContents
        ├── Permission and Privacy Services
        ├── Download Manager
        └── Local AI Bridge

The browser's existing Browser, TabStripModel, WebContents, LocationBarModel, navigation controller, permission manager, download manager and security-state services remain the source of truth. UI controls call those services; they do not reproduce them.

## 3. Split workspace

- The assistant is a child of the browser's native layout, not a fixed-position overlay.
- The main content region is laid out to the exact remaining width.
- The divider is draggable and keyboard accessible.
- A minimum content width prevents the webpage from collapsing to a narrow strip.
- The dock is hidden/shown without destroying the chat or current tab.
- Left/right placement is a persisted preference.
- Floating mode is a separate native browser-owned window and shares the same conversation model.
- "Always on top" is a Windows-only optional behavior; it is off by default and must be visibly indicated when on.

## 4. Tabs and navigation

Native Chromium tab/navigation state remains canonical. Sk/Ai replaces the presentation and command routing but not:
- session restore;
- tab lifecycle;
- tab drag/drop and tear-off;
- site isolation;
- omnibox navigation and autocomplete;
- download and permission models;
- certificate indicators and security interstitials.

Omnibox input uses Chromium's normal navigation/search pipeline. The chosen search provider is a user preference, independent of AI mode. The default may be Yandex; alternatives remain selectable.

## 5. Soft Aurora native design tokens

The same palette must be shared across native Views, the New Tab page and assistant UI.

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

Local AI remains mint. Cloud AI remains lavender. Color is semantic, not merely decorative. Use Chromium ColorIds/ColorProvider mixers for persistent native UI colors; direct SkColor values are reserved for transient effects.

## 6. Privacy and AI

- The local inference process binds only to 127.0.0.1.
- Local prompts/page snippets/files are sent only to the local model process.
- The assistant receives page context only after explicit user action.
- Password inputs, payment fields, cookies, browser history and hidden form values are never included by default.
- A page's text is untrusted data; it cannot override assistant system rules.
- Cloud mode remains a disabled placeholder until provider selection, secret storage, clear data disclosure and per-request user approval exist.
- No conversation is uploaded automatically when the mode switch changes.

## 7. GOST preservation

The native UI patch series must not modify the upstream GOST transport implementation unless a separate security-reviewed requirement demands it.

Keep the Chromium-Gost source pin, msspi submodule, BoringSSL patch and Chromium patch sequence intact. A Sk/Ai shell patch is applied after upstream chromium-gost-prepare.bat completes. We must test:
- ordinary TLS sites;
- GOST TLS sites;
- client certificate selection;
- certificate errors/interstitials;
- CryptoPro CSP compatibility where installed.

No test may disable certificate validation as a shortcut.

## 8. Process and memory model

- No browser-wide AI service is started in the background unless the user enables AI or opens the assistant.
- Qwen CPU inference is throttled and limited on the 8 GB target.
- The 2B model is optional at install time and can be installed later.
- Inactive tabs can be suspended using Chromium's native lifecycle/discard mechanisms.
- Browser UI remains responsive while inference runs.
- Model errors do not block navigation or crash browser processes.

## 9. Delivery gates

1. Source gate: pinned source checkout and upstream GOST patches complete.
2. Baseline gate: upstream Chromium-Gost compiles from source with no Sk/Ai patches.
3. UI gate: first native Soft Aurora chrome patch compiles and opens.
4. Navigation gate: tabs, omnibox, restore, downloads and permissions work.
5. Assistant gate: docked assistant resizes content and supports floating mode.
6. GOST gate: regression tests for TLS and CSP pass.
7. Performance gate: acceptable startup, RAM and tab lifecycle on 8 GB machines.
8. Release gate: signed, reproducible Windows installer and update channel.
