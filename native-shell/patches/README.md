# Chromium source patches

Place reviewed, version-pinned unified Git patches here. Files are applied in lexical order to pristine upstream Chromium 156.0.8078.88.

- 0001-skai-native-color-pipeline.patch
- 0002-skai-native-branding.patch
- 0003-skai-toolbar-and-tab-geometry.patch
- 0004-skai-native-assistant-toolbar-button.patch
- 0005-skai-browser-frame.patch
- 0006-skai-docked-assistant.patch

Rules:
- Every patch targets the clean upstream Chromium source tag pinned in native-shell/README.md.
- CI runs git apply --check and applies the entire ordered patch series to an unmodified Chromium source fixture.
- Keep each patch small and reviewable; compile-test native C++ changes before treating them as implemented.
- Do not patch out certificate validation, TLS verification, sandboxing, Safe Browsing or site isolation.
- No GOST, msspi or CryptoPro integration is in scope.
- A passing patch apply-check is not a compiled browser.
