# Upstream Chromium Windows Source Build

The native Sk/Ai browser is developed by modifying upstream Chromium. No Chromium-Gost repository or GOST/CryptoPro patch layer is used.

## Build host

- Windows 11 x64 on NTFS.
- At least 100 GB free disk; 120 GB is the bootstrap safety threshold.
- 8 GB RAM is the documented minimum; more than 16 GB is highly recommended. 32 GB is preferable for practical builds.
- Visual Studio 2026 (18.x) with Desktop development with C++, MSVC x64/x86 tools and MFC/ATL support.
- Git for Windows and depot_tools.
- A short workspace path without spaces, such as D:\SkAiBuild.

The 8 GB everyday computer is the intended runtime target, not the recommended Chromium compilation host.

## Bootstrap

Run from PowerShell:

    pwsh -File .\native-shell\bootstrap.ps1 -Workspace D:\SkAiBuild -ChromiumRef 156.0.8078.88 -InitializeChromiumCheckout

This clones depot_tools, checks out the upstream Chromium source, pins tag 156.0.8078.88, synchronizes DEPS and verifies chrome/VERSION.

The initial Chromium source download is large. Do not run it on a disk with less than the required free space.

## Compile pristine Chromium baseline

    pwsh -File .\native-shell\build.ps1 -Workspace D:\SkAiBuild -ChromiumRef 156.0.8078.88 -BaselineOnly

The baseline compiles unmodified upstream Chromium. It is a build-host check, not a Sk/Ai release.

## Compile the Sk/Ai patch overlay

Once the ordered Sk/Ai patch series passes CI:

    pwsh -File .\native-shell\build.ps1 -Workspace D:\SkAiBuild -ChromiumRef 156.0.8078.88

The script applies patches in lexical order, generates GN files with an x64 release-like configuration, builds the Chromium chrome target with autoninja and packages the runtime output folder as a ZIP.

The resulting package is still an engineering build. It is not a signed installer, and the assistant/model integration must pass its own gates before it can be called a usable browser.

## Remote build runner

The GitHub Actions workflow supports a privately configured self-hosted runner labelled:

    self-hosted, windows, x64, skai-chromium-large

The runner must have the disk, RAM, Visual Studio and network access required by Chromium. Do not put personal certificate stores, signing keys or user profile data on the runner.

Hosted Windows runners are used for JavaScript syntax checks, PowerShell parsing and exact-source patch apply-checks. They are not assumed to have enough disk space for a full Chromium source checkout and build.

## Reproducibility

- The Chromium tag is pinned in native-shell scripts and the CI patch validator.
- The exact chrome/VERSION file is checked before GN generation.
- Sk/Ai patch files remain separate, ordered and reviewed.
- Updating the Chromium version requires checking all patch contexts and CI before changing the pin.
