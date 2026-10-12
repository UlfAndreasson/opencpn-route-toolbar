# Platform status

Route Toolbar 0.5.9 currently targets OpenCPN 5.14.2, plugin API 1.21 and wxWidgets 3.2.

## Verified runtime

- macOS Apple Silicon (arm64): successfully imported through OpenCPN Plugin Manager and tested in OpenCPN 5.14.2.

## CI-built and package-validated

The following targets compile successfully in GitHub Actions and their Plugin Manager package layout/metadata is validated in CI, but still require runtime testing by beta users on the corresponding platform:

- macOS Intel x86_64
- macOS universal arm64 + x86_64
- Windows x86_64, MSVC, wxWidgets 3.2
- Ubuntu 24.04 x86_64, GTK3, wxWidgets 3.2

CI artifacts include Plugin Manager test tarballs for macOS universal, Windows x86_64 and Ubuntu 24.04 x86_64.

Passing CI confirms compilation, architecture, package structure and basic dependency checks. It does not replace real OpenCPN runtime testing on each platform.
