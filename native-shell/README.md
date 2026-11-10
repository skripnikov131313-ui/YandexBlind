# Sk/Ai Native Browser Shell

This directory is the beginning of the **native Sk/Ai browser**, separate from the earlier start-page extension prototype.

## Goal

Replace the stock Chromium browser chrome with Sk/Ai's own Windows browser interface while retaining the Chromium-Gost browser engine and its GOST/msspi integration.

The product is not a new HTML renderer or a new JavaScript engine. It is a native browser application built by modifying Chromium's own browser UI layer. This preserves the mature rendering, networking, process isolation, certificate and web-platform implementation rather than reimplementing them.

## Pinned source baseline

- Chromium-Gost source tag: **156.0.8078.88**
- Upstream: https://github.com/deemru/Chromium-Gost
- Windows patch/build scripts: upstream build_windows/
- GOST transport integration: upstream src/gostssl.cpp, src/msspi/, and their corresponding Chromium/BoringSSL patches.

The existing distribution uses Chromium-Gost 156.0.8078.88. We pin that same source tag for the first reproducible source build instead of silently moving to a different upstream revision. Updating the pin is a separate, reviewed change.

## Browser layout

The default window follows the supplied Sk/Ai reference:

1. Native Windows title controls and Sk/Ai top chrome.
2. Native tab strip.
3. Sk/Ai navigation row with Back, Forward, Reload, secure-connection status, omnibox, bookmarks, downloads and menu.
4. A narrow vertical navigation rail on the left.
5. The active website/search page in the main content region.
6. A docked Sk/Ai Assistant on the right, sharing the window with the page rather than covering it.

The assistant can be:
- docked on the right (default);
- docked on the left (user preference);
- detached into a separate resizable Windows window;
- minimized, maximized, restored or re-docked;
- optionally kept above other Windows windows, but only after the user enables that native Windows behavior.

The webpage must resize to the remaining content region when the assistant is docked. It must never be obscured by a modal assistant overlay.

## Native implementation direction

The shell belongs in Chromium's C++ Views browser UI, not in a full-screen extension page.

Primary integration points in Chromium 156:
- chrome/browser/ui/views/frame/browser_view.h/.cc — top-level browser view, chrome/content layout and side-panel ownership.
- chrome/browser/ui/views/frame/browser_view_layout.* — layout constraints and available bounds.
- chrome/browser/ui/views/frame/tab_strip_region_view.* — tab row and new-tab affordance.
- chrome/browser/ui/views/toolbar/toolbar_view.* — navigation controls, location bar and toolbar buttons.
- chrome/browser/ui/views/side_panel/* — docked side-panel infrastructure.
- chrome/browser/ui/color/* — Chrome color mixers and ColorIds for the Soft Aurora native palette.

We will extend these existing subsystems rather than build a parallel tab, navigation, security or WebContents implementation.

## AI boundary

Local Qwen remains a separate CPU process bound to loopback only. The native assistant UI calls the local inference service and passes page context only after an explicit user action. Cloud mode remains unavailable until a provider, key storage and per-request consent UI are implemented.

Page text, files and web content are untrusted context, not instructions. Password fields, form values, cookies and browser profile data must never be scraped into the assistant context.

## Source build

Chromium's Windows source build needs a dedicated build machine. Current Chromium guidance asks for at least 100 GB free NTFS disk and recommends more than 16 GB RAM; the official current Windows toolchain is Visual Studio 2026. The user's 8 GB daily-use PC is not our build server.

The scripts here:
- check build prerequisites;
- clone the pinned Chromium-Gost patch repository and depot_tools;
- optionally initialize a Chromium checkout;
- generate the upstream private environment override without changing upstream tracked files;
- run the pinned upstream GOST preparation step;
- apply Sk/Ai patches from native-shell/patches/*.patch;
- optionally build the upstream baseline for comparison.

The first milestone is reproducible source preparation and baseline build. The first native Soft Aurora color-mixer patch is in native-shell/patches/0001-skai-native-color-pipeline.patch and passed the exact-source apply-check on Windows CI. A second patch changes Chromium's product identity to Sk/Ai in native-shell/patches/0002-skai-native-branding.patch; its apply-check is now included in the same ordered CI validation. Neither patch has been compiled or runtime-validated yet. Further native UI patches will be introduced in small, reviewable, compile-tested steps.

## Current state

- [x] Separate native-shell work branch.
- [x] Source baseline pinned to the current project Chromium-Gost version.
- [x] Chromium-Gost preparation workflow inspected.
- [x] Native integration points mapped.
- [x] Bootstrap/build/validation scripts added.
- [ ] Full Chromium source checkout on a large Windows build machine.
- [ ] Baseline Chromium-Gost source build.
- [x] First Sk/Ai ColorProvider patch added for Chromium 156 (exact-source apply-check passed in CI).
- [x] Native product branding patch added (Sk/Ai product and installer names; next CI apply-check pending).
- [x] Native tab corner radii and toolbar / omnibox spacing refinement added (CI apply-check pending).
- [x] Native C++ Assistant toolbar button added; it opens the stable Sk/Ai extension entry in Chromium's docked Side Panel (CI apply-check pending).
- [ ] Native docked assistant registration.
- [ ] Floating Windows assistant and Always-on-Top option.
- [ ] Windows 11 integration and performance validation.

A green script-validation job is not a compiled browser. We will report source preparation, compilation and runtime tests separately.
