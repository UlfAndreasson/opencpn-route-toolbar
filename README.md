# OpenCPN Route Toolbar

Route Toolbar is an OpenCPN passage-planning plugin for practical, route-aware
pilotage tools.

It combines:

- EBL / VRM sight lines
- Parallel Index Lines (PIL)
- route-leg course/distance labels
- persistent/editable planning objects
- PDF and CSV route reports
- complete GPX package export/import

## Current version

**Route Toolbar 0.4.1**

Current development target:

- OpenCPN 5.14.2
- OpenCPN plugin API 1.21
- wxWidgets 3.2
- macOS Apple Silicon

## Route leg labels

Each OpenCPN route leg can be annotated directly on the chart with true course
and distance using the compact format:

```text
128°T  0.84 M
```

The label is placed above the route leg when screen space permits. If the route
has an explicit OpenCPN route colour, the label uses the same colour. Routes
using OpenCPN's default colour use the matching OpenCPN route display colour.

## Route EBL / VRM

An EBL/VRM can originate from:

- an OpenCPN route waypoint, or
- a standalone OpenCPN Mark.

The second point is a visual/radar sight target such as a lighthouse, buoy,
beacon, cardinal mark or other conspicuous object.

Route Toolbar displays:

- EBL line
- target ring/crosshair
- true bearing
- range in NM
- compact VRM arc
- yellow information label

The default VRM arc is 30 degrees total, centred on the EBL bearing.

### Mark-based EBL/VRM

For an EBL originating from a normal OpenCPN Mark, the Mark GUID is stored.

If the Mark is moved in OpenCPN, Route Toolbar follows the new position and
recalculates bearing and range.

### EBL label handling

EBL labels support:

- configurable font size
- stable automatic placement
- manual label placement
- persistent manual position
- reset to automatic position
- labels hidden when the target is outside the visible chart area
- global **Hide all yellow notes / View all yellow notes** control in Route Toolbar

Hiding yellow notes leaves EBL/VRM and PIL geometry visible so the chart can be
used without the information boxes obscuring the route.

### EBL/VRM editing

An existing EBL/VRM can be selected from its start point, target/end point or
the EBL line itself. Chart context-menu editing no longer requires clicking the
target crosshair/arc.

Available actions include:

- Move start point
- Move target
- Move label
- Reset label position
- Rename
- Delete

Moving a start point graphically makes that EBL/VRM start a free chart position;
it is no longer tied to its original route waypoint or Mark.

## Route PIL

Route PIL attaches a Parallel Index Line directly to an existing OpenCPN route
leg.

Workflow:

1. Activate **Route PIL**
2. Click the route leg
3. Click the geographic point through which the parallel line should pass

The plugin calculates the perpendicular offset from the route leg.

Offset convention:

- positive = **SB / starboard**
- negative = **BB / port**

Multiple PILs can exist on one route leg.

PIL objects are persistent and can be moved graphically or deleted.

## Colours

Route Toolbar provides separate colour settings for:

- EBL / VRM
- PIL

The settings are persistent.

## Route Report

Route Report exports the OpenCPN route together with Route Toolbar planning
data.

Formats:

- **PDF — A4 landscape**
- **CSV — Numbers / Excel / spreadsheet**

The report includes:

- route name
- total route distance
- waypoint number/name
- waypoint latitude/longitude
- course to next waypoint
- leg distance
- PIL information
- sight-mark name
- EBL true bearing
- EBL range
- sight-mark latitude/longitude

PIL information is displayed as separate rows, for example:

```text
SB: 0.02 NM
BB: 0.03 NM
```

EBL/VRM objects which originate from standalone OpenCPN Marks are included in a
separate report section.

## Route GPX

**Route GPX...** provides complete route-package export/import.

The exported file is a normal GPX 1.1 document containing:

- OpenCPN route
- route waypoints
- referenced standalone Marks

Route Toolbar data is stored in a namespaced GPX `<extensions>` section.

A complete package includes:

- route and waypoint GUIDs
- referenced Mark GUIDs
- EBL/VRM objects
- sight-target names and positions
- VRM arc width
- manual EBL label positions
- PIL offsets and geometry
- EBL/PIL colours
- EBL label size

Import supports:

- **Import as new**
- **Replace existing**

This makes GPX the portable backup and transfer format for a complete Route
Toolbar passage plan.

See [docs/GPX_FORMAT.md](docs/GPX_FORMAT.md).

## Persistence

Normal working state is persisted in OpenCPN configuration, including yellow
note visibility.

Route Toolbar intentionally loads navigation-linked state a few seconds after
OpenCPN startup. This allows OpenCPN routes, waypoints and Marks to be fully
initialized before their GUIDs are resolved.

## Design goals

1. Route-aware
2. Few clicks
3. Clear chart display
4. Persistent
5. Editable
6. Reportable
7. Portable
8. Non-destructive

Route Toolbar must not silently alter an OpenCPN route during normal EBL/PIL
planning.

## Build

See [docs/BUILDING.md](docs/BUILDING.md).

Quick Apple Silicon build:

```bash
rm -rf build

WX32="$(brew --prefix wxwidgets@3.2)/bin/wx-config-3.2"

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DwxWidgets_CONFIG_EXECUTABLE="$WX32"

cmake --build build -j 4
```

## Documentation

- [Functional specification](docs/SPEC.md)
- [Build instructions](docs/BUILDING.md)
- [GPX package format](docs/GPX_FORMAT.md)
- [Changelog](CHANGELOG.md)

## Upstream references

- OpenCPN: https://github.com/OpenCPN/OpenCPN
- OCPN Draw: https://github.com/jongough/ocpn_draw_pi
- OpenCPN plugin API libraries: https://github.com/OpenCPN/opencpn-libs

OCPN Draw has been an important reference for navigation drawing concepts,
persistence and GPX handling.

## Navigation notice

Route Toolbar is passage-planning support software.

It does not replace official charts, prudent navigation practice or independent
position verification.
