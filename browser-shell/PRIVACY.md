# Sk/Ai website privacy protections

This document describes protections applied to requests made by websites in browser tabs. It is separate from the application's own data handling.

## Application-side collection

The native-shell source does not include an analytics SDK, an application telemetry endpoint, an advertising SDK, or a mechanism that uploads browser history, keystrokes or browsing activity. Local history, bookmarks, tabs and assistant conversations are browser features stored in the local profile. No new application-side collection has been added for this privacy work.

## Enabled website protections

- **Third-party cookies:** each persistent CEF profile sets Chromium's `profile.cookie_controls_mode` to `1` (`kBlockThirdParty`). First-party cookies remain available so normal site sessions and sign-ins are not intentionally erased.
- **Tracker request filter:** requests to a curated set of known third-party analytics, advertising, attribution, session-replay and pixel hosts are cancelled before network loading.
- **Cross-site beacon filtering:** common collection, pixel, beacon and telemetry URL paths are cancelled only when the request is cross-site. Same-site API paths are not blocked solely because their name contains "analytics" or "collect".
- **Global Privacy Control:** HTTP(S) requests carry `Sec-GPC: 1`, an opt-out signal that participating sites may honor. It is advisory and is not a substitute for blocking.

The initial host list includes Google Analytics and advertising endpoints, DoubleClick, Bing Ads/Clarity, Hotjar, Mixpanel, Amplitude, Heap, FullStory, Contentsquare, Mouseflow, Crazy Egg, Chartbeat, Parsely, common ad exchanges and several session-attribution providers. Broad hostnames such as `google.com`, `facebook.com` and `linkedin.com` are not blanket-blocked.

## Deliberate limits

- A site can still collect data through its own first-party domain, server logs, account activity or a first-party analytics endpoint. This phase does not inspect or rewrite arbitrary first-party scripts or API payloads.
- CNAME-cloaked trackers and newly created tracking hosts are not reliably detected by the curated host list.
- Third-party cookie blocking reduces cross-site cookie identifiers but does not eliminate all storage-based or fingerprint-based identification.
- First-party cookies are not automatically cleared. Automatically erasing all first-party cookies would sign users out of websites and destroy normal sessions.
- Canvas, WebGL, User-Agent and Client Hints, font metrics, viewport dimensions and timing APIs are not spoofed or blanket-disabled in this phase. Those APIs are used by legitimate sites, and naive spoofing can break sites or produce a more distinctive browser fingerprint.
- Chromium Safe Browsing, certificate validation, the renderer sandbox and security updates are not disabled.

Complete elimination of browser fingerprinting cannot be promised by request filtering or cookie settings alone. The next privacy phase will assess individual high-entropy APIs and storage partitioning separately, with compatibility checks and site-level exceptions before any broader restrictions are enabled.

## Maintenance

The initial domain rules are compiled into `src/skai_privacy.cpp`. Changes to the list should be reviewed for false positives and tested against login flows, payments, video, maps and common productivity sites before release. The list is intentionally curated rather than downloaded without review.
