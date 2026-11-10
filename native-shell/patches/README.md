# Chromium source patches

Place reviewed, version-pinned unified Git patches here:

- 0001-skai-native-color-pipeline.patch
- 0002-skai-native-branding.patch
- 0003-skai-toolbar-and-tab-geometry.patch
- 0004-skai-browser-frame.patch
- 0005-skai-docked-assistant.patch

Patches are applied in lexical order after the pinned upstream Chromium-Gost preparation step and before GN generation.

Rules:
- Every patch targets the pinned Chromium source tag in native-shell/README.md.
- Test with git apply --check before applying.
- Keep GOST/msspi/BoringSSL files outside the Sk/Ai patch series.
- Add or update Chromium browser/UI unit and browser tests with each behavior patch.
- Never patch out certificate validation, sandboxing, Safe Browsing or origin isolation.
- The first source milestone compiles an unmodified Chromium-Gost baseline; no Sk/Ai native UI patch is claimed to exist yet.
