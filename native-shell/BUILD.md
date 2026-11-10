# Windows source build prerequisites

The native shell is built inside Chromium-Gost's source tree. This is separate from the existing lightweight packaging workflow.

## Machine

- Windows 11 x64 on an NTFS volume.
- 120 GB free disk space recommended for the workspace (Chromium documentation currently lists at least 100 GB).
- More than 16 GB RAM strongly recommended; 32 GB is a safer practical target for builds.
- Visual Studio 2026 (18.x) with Desktop development with C++, MSVC x64/x86 tools and a Windows SDK.
- Git for Windows.
- Python supplied/managed by depot_tools.
- 7-Zip at the default upstream build-script path, or adjust the generated private environment file.

The 8 GB target computer is the runtime target, not the intended compilation machine.

## Workspace

Use a short path without spaces, for example:

    D:\SkAiBuild
    ├── depot_tools
    ├── chromium
    │   └── src
    └── Chromium-Gost

The upstream Windows scripts contain unquoted legacy batch variables and expect a path without spaces. The bootstrap script enforces this to avoid path-splitting failures.

## Initial source checkout

The first source download is large. It is explicit; do not run it on the user's 8 GB daily-use computer.

    pwsh -File .\native-shell\bootstrap.ps1 -Workspace D:\SkAiBuild -ChromiumGostRef 156.0.8078.88 -InitializeChromiumCheckout

This clones the pinned Chromium-Gost source/patch repository and depot_tools. It then uses depot_tools' fetch chromium to create the Chromium source checkout. The upstream prepare script subsequently checks out the pinned Chromium tag and synchronizes dependencies.

## Prepare source and compile upstream baseline

    pwsh -File .\native-shell\build.ps1 -Workspace D:\SkAiBuild -ChromiumGostRef 156.0.8078.88 -BaselineOnly

This runs upstream chromium-gost-prepare.bat and chromium-gost-build-release.bat. It produces an upstream Chromium-Gost baseline with the Sk/Ai source branch's current UI patches (none yet). It is a baseline artifact, not a finished Sk/Ai browser.

Once native patch files exist in native-shell/patches/*.patch, omit -BaselineOnly to apply them before compilation.

## Remote build runner

The GitHub Actions workflow supports a manually configured self-hosted Windows runner labelled:

self-hosted, windows, x64, skai-chromium-large

The runner must satisfy the machine requirements above and have permission to access GitHub and the upstream Chromium source/dependency hosts. Configure it privately; do not store source tree credentials or CSP keys in repository secrets.

A standard hosted Windows runner is suitable for lint and small CI checks, but not for a complete Chromium checkout/build on a 14 GB-class runner disk.

## Version discipline

The current project distribution and this first source-build branch are pinned to Chromium-Gost 156.0.8078.88. The upstream repository's default branch can advance independently. Never replace the pin with master or latest in a reproducible build.
