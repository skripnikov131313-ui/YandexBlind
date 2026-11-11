# Sk/Ai Privacy Model

## Non-negotiable invariants

- The browser shell contains no analytics SDK, session recorder, advertising SDK, app-generated tracking identifier or silent crash-report upload.
- No site receives camera, microphone, geolocation, notification or clipboard permission without an explicit browser decision.
- No consent API may report that the user granted advertising, analytics or personalization consent unless the user actually made that choice. The compatibility default is denial.
- No compatibility shim may send an ad impression, click, conversion, analytics or telemetry beacon.
- Local AI remains local. Cloud transfer stays disabled until an explicit provider and a per-transfer consent preview exist.
- GOST TLS, Russian certificate overrides and CryptoPro CSP remain out of scope.

## Advertising compatibility

1. Native request filtering cancels known third-party ad and measurement endpoints.
2. The renderer compatibility shim provides a limited Google Publisher Tag surface. Slot lifecycle callbacks report an empty slot (`isEmpty: true`); they do not claim that an ad was served.
3. AdSense queue calls and common analytics entry points are no-ops. They exist only to prevent missing-global errors in sites that test for these APIs.
4. Common ad creative frames/elements are hidden locally. No ad creative is rendered.
5. Never synthesize a successful paid impression, click, conversion or billing event. If a site gates content on a verified paid impression, the browser cannot honestly satisfy that condition without displaying and loading the ad.

## Consent and cookies

- The local IAB TCF compatibility response is denial, not acceptance. Represented purposes are false; Global Privacy Control is exposed where supported.
- A site's consent banner must not be auto-accepted or have an affirmative consent cookie fabricated.
- Third-party cookies are blocked in Chromium.
- A CEF cookie access filter blocks known advertising/analytics cookie names when cookies are saved from responses or sent with requests, including top-level navigations.
- First-party cookies not matched by the known tracker-name list currently remain available for site sessions. This is a known privacy gap: first-party identifiers can use arbitrary names. A later cookie policy must distinguish session-critical state from analytics/advertising state, use ephemeral storage where possible, and expose any site exception to the user.
- A JavaScript-only fake cookie jar is not equivalent to a server session. It cannot authenticate a user or satisfy a server-side session requirement when the server expects a real cookie on an HTTP request.

## Telemetry and tracking

- Known third-party ad, analytics, session-replay and fingerprinting endpoints are blocked in native CEF request handling.
- Known marketing/click-attribution query parameters are removed from top-level HTTP(S) GET navigations; POST/form bodies and functional query parameters are not rewritten.
- No blanket URL-path heuristic may block generic endpoints such as `/events` or `/collect` without evidence that the endpoint is telemetry; those names are also used by legitimate application APIs.
- First-party telemetry, storage-based identifiers, service-worker behavior and advanced JavaScript fingerprinting remain areas for further implementation and testing.
- Browser-level protection cannot prevent a website's own server from seeing the IP address and ordinary request metadata required to serve a connection. Do not describe the browser as making users untrackable.

## Compatibility boundaries

Local API stubs can keep some pages from failing when they expect an ad, analytics or consent API global to exist. They cannot guarantee that a site will provide content when it requires a genuine authenticated session, a server-validated consent record, a real ad response or an impression verified by an external ad network.

## Acceptance gates

- Native request tests cover exact host boundaries, subdomains, false-positive domains, redirects and downloads.
- Renderer tests verify that ordinary HTTP(S) contexts receive the shim while the trusted Sk/Ai UI does not.
- TCF tests verify denial; no code path may synthesize affirmative consent.
- Ad tests verify empty-slot callbacks and the absence of impression/click/beacon requests.
- Cookie tests must inspect on-disk persistence, session restart behavior and first-party versus third-party requests.
- Windows CI and real-site runtime tests are required before any item is marked complete.
