# Building Route Toolbar on macOS

Current development target:

- OpenCPN 5.14.2-universal
- Plugin API 1.21
- wxWidgets 3.2
- Apple Silicon arm64
- Route Toolbar 0.5.4

## Prerequisites

Install:

```bash
brew install cmake wxwidgets@3.2
```

Xcode Command Line Tools are also required.

Do not build Route Toolbar against wxWidgets 3.3 for OpenCPN 5.14.2. OpenCPN 5.14.2 uses wxWidgets 3.2; loading another incompatible wx runtime into the same process can cause crashes when plugin windows are created.

## Clone

```bash
git clone --recurse-submodules \
  https://github.com/UlfAndreasson/opencpn-route-toolbar.git

cd opencpn-route-toolbar
```

Existing checkout:

```bash
git submodule update --init --recursive
```

## Configure

Apple Silicon:

```bash
rm -rf build

WX32="$(brew --prefix wxwidgets@3.2)/bin/wx-config-3.2"

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DwxWidgets_CONFIG_EXECUTABLE="$WX32"
```

Route Toolbar uses wxWidgets core, base and xml. XML is required for GPX package handling.

## Build

```bash
cmake --build build -j 4
```

Successful result:

```text
[100%] Built target route_toolbar_pi
```

macOS produces OpenGL deprecation warnings for legacy calls such as `glBegin`, `glEnd` and `glVertex*`. These warnings are expected and are not build failures.

## wxWidgets dependency rewrite

The post-build step rewrites Homebrew wxWidgets dependencies to the runtime inside OpenCPN.app and then ad-hoc signs the plugin.

Verify:

```bash
otool -L build/libroute_toolbar_pi.dylib | grep wx
```

Expected:

```text
@executable_path/../Frameworks/libwx_osx_cocoau_core-3.2.dylib
@executable_path/../Frameworks/libwx_baseu-3.2.dylib
@executable_path/../Frameworks/libwx_baseu_xml-3.2.dylib
```

No Homebrew `/opt/homebrew/...libwx...` path should remain.

## Development install

```bash
killall OpenCPN 2>/dev/null
sleep 2

cp build/libroute_toolbar_pi.dylib \
  "$HOME/Library/Application Support/OpenCPN/Contents/PlugIns/"

rm -f \
  "$HOME/Library/Preferences/opencpn/load_stamps/libroute_toolbar_pi"

open -n /Applications/OpenCPN.app
```

## Important paths

Plugin:

```text
~/Library/Application Support/OpenCPN/Contents/PlugIns/
```

Configuration:

```text
~/Library/Preferences/opencpn/opencpn.ini
```

Log:

```text
~/Library/Logs/opencpn.log
```

## Persistence warning

Do not delete `opencpn.ini` as a normal troubleshooting step. It contains persistent Route Toolbar planning data.

## Startup

Route Toolbar intentionally delays loading persisted EBL/VRM, PIL and TURN state for a few seconds after initialization. This prevents navigation GUID lookups from happening before OpenCPN has loaded routes and Marks.

## Load stamp recovery

If OpenCPN refuses to reload Route Toolbar after a failed plugin startup:

```bash
rm -f \
  "$HOME/Library/Preferences/opencpn/load_stamps/libroute_toolbar_pi"
```

Remove only the Route Toolbar load stamp.

## Source layout

```text
src/route_toolbar_pi.cpp
    Core plugin, EBL/VRM rendering, Route Objects and persistence

src/route_pil.cpp
    PIL geometry and interaction

src/route_turn.cpp
    TURN geometry and interaction

src/route_labels.cpp
    Route-leg course/distance labels

src/route_context.cpp
    Chart context-menu editing

src/route_report.cpp
    CSV report

src/route_report_pdf.cpp
    PDF report

src/route_gpx.cpp
    GPX package import/export

src/route_toolbar_dialog.cpp
    Route Toolbar window

include/route_toolbar_pi.h
include/route_toolbar_dialog.h
```
