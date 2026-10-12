# OpenCPN Route Toolbar

**Route-aware coastal passage planning directly on your existing OpenCPN route.**

Route Toolbar adds practical pilotage tools without requiring you to redraw the route as separate drawing objects. Work from the route already created in OpenCPN and add:

- route-aware **EBL / VRM** sight lines
- **Parallel Index Lines (PIL)** attached to route legs
- **TURN** geometry at route waypoints
- true-course / distance labels on every route leg
- persistent, editable planning objects
- PDF and CSV passage-plan reports
- portable GPX route packages with Route Toolbar data

![Route Toolbar passage-planning overview](screenshots/Screenshot%202026-10-12%20at%2016.03.54.png)

*Route-aware EBL/VRM, parallel index lines, turn geometry and leg information on an existing OpenCPN route.*

## Why Route Toolbar?

Many useful pilotage tools can be created with generic drawing tools, but they normally exist independently of the OpenCPN route itself. Route Toolbar is deliberately **route-aware**:

- select an existing route leg or waypoint instead of recreating it
- keep PIL, EBL/VRM and TURN planning linked to the route
- automatically annotate route legs with course and distance
- hide the OpenCPN route and its leg information disappears with it
- export the route and its planning data together
- keep OpenCPN as the owner/editor of the actual route and waypoints

The goal is not to replace general-purpose drawing plugins. It is to make **coastal passage planning and pilotage preparation quicker when the OpenCPN route is already the centre of the workflow**.

## Current version

**Route Toolbar 0.5.9**

Current development target:

- OpenCPN 5.14.2
- OpenCPN plugin API 1.21
- wxWidgets 3.2
- macOS Apple Silicon

> **Current status:** development preview. The macOS Apple Silicon package has been successfully imported through OpenCPN Plugin Manager. Catalog distribution, universal macOS packaging and wider platform builds are the next release steps.

## Typical workflow

1. Create or open your route in normal OpenCPN Route Manager.
2. Open **Route Toolbar**.
3. Add EBL/VRM sight lines, PILs and planned turns directly from route geometry.
4. Review or temporarily hide planning objects from **Route Objects...**.
5. Export a PDF/CSV route report or a complete GPX package.

![Route Toolbar main window](screenshots/Screenshot%202026-10-12%20at%2016.04.04.png)

*The main Route Toolbar keeps the passage-planning tools together without replacing OpenCPN's own route editor.*

## Route leg labels

Each OpenCPN route leg is annotated with true course and distance using the compact format:

```text
128°T  0.84 M
```

The label follows the route-leg angle and is placed immediately above shallow legs or immediately to the visual left of steep legs. Leg info is shown only while the source OpenCPN route itself is visible, and it also follows the global **Hide/View notes and leg info** control.

OpenCPN API 1.21 does not expose every per-route custom display colour to plugins, so Route Toolbar uses the normal OpenCPN route display colour available through the plugin API.

## Route EBL / VRM

An EBL/VRM can start at an exact free chart position, or be Shift-snapped to a nearby route waypoint, OpenCPN Mark or Route TURN start/end point. The target is a visual/radar sight target such as a lighthouse, buoy, beacon, cardinal mark or other conspicuous object.

Route Toolbar displays the EBL line, target ring/crosshair, true bearing, range, compact VRM arc and an optional yellow information note.

### EBL/VRM editing

An existing EBL/VRM can be selected from its start point, target/end point or the EBL line itself. Available actions include:

- Move start point
- Move target
- Move label
- Reset label position
- Rename
- Delete

Moving a start point graphically makes it a free chart position and removes its previous waypoint/Mark/TURN snap association.

![Editing a Route EBL/VRM](screenshots/Screenshot%202026-10-12%20at%2016.04.14.png)

*Existing EBL/VRM objects can be edited directly from the chart.*

## Route PIL

Route PIL attaches a Parallel Index Line directly to an existing OpenCPN route leg.

Workflow:

1. Activate **Route PIL**
2. Click the route leg
3. Click the geographic point through which the parallel line should pass

Offset convention:

- positive = **SB / starboard**
- negative = **BB / port**

Multiple PILs can exist on one route leg. PIL objects are persistent, graphically editable and rendered with a compact dashed pattern.

## Route TURN

Route TURN creates tangent turn geometry at an interior route waypoint. The user supplies a geometric turn radius; Route Toolbar calculates the course change, wheel-over/turn-start position, tangent arc and turn-end position. A planned rudder angle can be stored as a manual planning value.

TURN objects are persistent and can be used as EBL/VRM snap points. TURN data is included in Route Toolbar GPX package data.

## Route Objects

**Route Objects...** lists saved EBL/VRM, PIL and TURN objects immediately when the dialog opens. Objects can be multi-selected, selected all at once, shown, hidden or deleted without deleting the source OpenCPN route.

![Route Objects manager](screenshots/Screenshot%202026-10-12%20at%2016.05.10.png)

*Route Objects provides one place to inspect, show, hide and manage the planning objects associated with the route.*

## Notes and leg info

The toolbar provides **Hide notes and leg info / View notes and leg info**. Hiding them leaves EBL/VRM, PIL and TURN geometry visible, making it easier to inspect the route without information boxes or leg labels obscuring the chart. The setting is persistent.

## Colours

Route Toolbar provides persistent colour settings for:

- EBL / VRM
- PIL
- TURN

## Route Report

Route Report exports the OpenCPN route together with Route Toolbar planning data.

Formats:

- **PDF — A4 landscape**
- **CSV — Numbers / Excel / spreadsheet**

The report includes route/waypoint information, course and distance, PIL offsets and EBL/VRM sight information.

![Route Report CSV opened in Numbers](screenshots/Screenshot%202026-10-12%20at%2016.05.50.png)

*CSV route report opened in Numbers, combining route legs, PIL and EBL/VRM passage-planning information.*

## Route GPX

**Route GPX...** provides complete route-package export/import. The exported file is a normal GPX 1.1 document with Route Toolbar data in a namespaced `<extensions>` section.

A package can contain route/waypoint identity, referenced Marks, EBL/VRM objects, PIL geometry, TURN geometry, colours and label settings.

Import behaviour:

- if the route does not exist in OpenCPN, the route, waypoints and Route Toolbar objects are imported
- if the route already exists and has no Route Toolbar objects, only the EBL/PIL/TURN package is attached to the existing route, without prompting or rewriting the route/waypoints
- if the route already exists and already has Route Toolbar objects, **Replace existing** replaces only the Route Toolbar package while **Import as new** creates a new route copy

OpenCPN remains the owner/editor of routes and waypoints.

See [docs/GPX_FORMAT.md](docs/GPX_FORMAT.md).

## Persistence

Normal working state is persisted in OpenCPN configuration. Route Toolbar intentionally loads navigation-linked state a few seconds after OpenCPN startup so routes, waypoints and Marks are initialized before their GUIDs are resolved.

## Design goals

1. Route-aware
2. Few clicks
3. Clear chart display
4. Persistent
5. Editable
6. Reportable
7. Portable
8. Non-destructive

Route Toolbar must not silently alter an OpenCPN route during normal planning.

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

## Public release / OpenCPN community

See [docs/PUBLIC_RELEASE.md](docs/PUBLIC_RELEASE.md) for the planned path from the current development preview to an OpenCPN Plugin Manager beta release.

## Documentation

- [Functional specification](docs/SPEC.md)
- [Build instructions](docs/BUILDING.md)
- [GPX package format](docs/GPX_FORMAT.md)
- [Public release plan](docs/PUBLIC_RELEASE.md)
- [Changelog](CHANGELOG.md)

## Upstream references

- OpenCPN: https://github.com/OpenCPN/OpenCPN
- OCPN Draw: https://github.com/jongough/ocpn_draw_pi
- OpenCPN plugin API libraries: https://github.com/OpenCPN/opencpn-libs

## Navigation notice

Route Toolbar is passage-planning support software. It does not replace official charts, prudent navigation practice or independent position verification.
