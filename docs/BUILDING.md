# Building Route Toolbar on macOS

The current scaffold targets OpenCPN plugin API **1.18**, which is compatible with OpenCPN 5.12.x and newer hosts which keep API 1.18 compatibility.

## Prerequisites

- Xcode command-line tools
- CMake
- wxWidgets 3.2 built for the same CPU architecture as OpenCPN

With Homebrew:

```bash
brew install cmake wxwidgets
```

## Clone

```bash
git clone --recurse-submodules https://github.com/UlfAndreasson/opencpn-route-toolbar.git
cd opencpn-route-toolbar
```

If already cloned:

```bash
git submodule update --init --recursive
```

## Configure and build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
```

Expected macOS output:

```text
build/libroute_toolbar_pi.dylib
```

The first milestone is successful when OpenCPN loads the plugin and a **Route Toolbar** button opens a small window containing:

- Route EBL / VRM
- Route PIL
- Route Report

These buttons are UI scaffolding only in v0.1.0; chart interaction is implemented in the next milestones.

## Architecture note

The plugin, OpenCPN and wxWidgets must use compatible architectures (for example all `arm64` on Apple Silicon). If CMake selects the wrong architecture, configure explicitly, e.g.:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
```

## OpenCPN API dependency

`opencpn-libs` is included as a git submodule and the build currently uses `opencpn-libs/api-18`.

## Development order

1. Verify plugin load + Route Toolbar UI on OpenCPN 5.12.2/macOS.
2. Add cursor/mouse capture and route/leg identification.
3. Implement Route EBL/VRM with a 90-degree range arc.
4. Implement Route PIL directly from a selected route leg.
5. Add persistence/manager integration.
6. Add Route Report CSV and printable HTML export.
