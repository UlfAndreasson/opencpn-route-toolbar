# Building Route Toolbar on macOS

The current development target is **OpenCPN 5.14.2** using plugin API **1.21**.

## Prerequisites

- Xcode command-line tools
- CMake
- **wxWidgets 3.2** built for the same CPU architecture as OpenCPN

Do not build this plugin against Homebrew's default `wxwidgets` formula when it resolves to wxWidgets 3.3. A plugin built against wxWidgets 3.3 can load into OpenCPN but crash when a wx window is created if OpenCPN is running wxWidgets 3.2.

With Homebrew:

```bash
brew install cmake wxwidgets@3.2
```

## Clone

```bash
git clone --recurse-submodules https://github.com/UlfAndreasson/opencpn-route-toolbar.git
cd opencpn-route-toolbar
```

If already cloned:

```bash
git pull
git submodule update --init --recursive
```

## Configure and build

On Apple Silicon:

```bash
rm -rf build
WX32="$(brew --prefix wxwidgets@3.2)/bin/wx-config-3.2"
"$WX32" --version
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DwxWidgets_CONFIG_EXECUTABLE="$WX32"
cmake --build build --config Release -j 4
```

Expected macOS output:

```text
build/libroute_toolbar_pi.dylib
```

The CMake build rewrites wxWidgets dependencies in the finished plugin to OpenCPN's bundled runtime:

```text
@executable_path/../Frameworks/libwx_...-3.2.dylib
```

Verify with:

```bash
otool -L build/libroute_toolbar_pi.dylib | grep wx
```

No `/opt/homebrew/.../libwx...` path should remain in the final plugin.

## Install development build

Quit OpenCPN first, then:

```bash
PLUGIN_DIR="$HOME/Library/Application Support/OpenCPN/Contents/PlugIns"
mkdir -p "$PLUGIN_DIR"
cp build/libroute_toolbar_pi.dylib "$PLUGIN_DIR/"
codesign --force --sign - "$PLUGIN_DIR/libroute_toolbar_pi.dylib"
```

Then start OpenCPN and enable **Route Toolbar** in Options -> Plugins.

The first milestone is successful when OpenCPN loads the plugin and a **Route Toolbar** button opens a small window containing:

- Route EBL / VRM
- Route PIL
- Route Report

These buttons are UI scaffolding only in v0.1.0; chart interaction is implemented in the next milestones.

## Architecture note

The plugin, OpenCPN and wxWidgets must use compatible architectures. OpenCPN 5.14.2-universal can host the native Apple Silicon build when running as `arm64`.

## OpenCPN API dependency

`opencpn-libs` is included as a git submodule and the build currently uses `opencpn-libs/api-21`.

## Development order

1. Verify plugin load + Route Toolbar UI on OpenCPN 5.14.2/macOS.
2. Add cursor/mouse capture and route/leg identification.
3. Implement Route EBL/VRM with a 90-degree range arc.
4. Implement Route PIL directly from a selected route leg.
5. Add persistence/manager integration.
6. Add Route Report CSV and printable HTML export.
