# Route Toolbar — Functional Specification

## Version 0.5.5 scope

Route Toolbar covers these primary workflows:

1. EBL/VRM passage-planning sight lines
2. Route-leg Parallel Index Lines (PIL)
3. Route TURN geometry
4. Route-leg course/distance chart labels
5. Route Objects visibility/management
6. PDF/CSV Route Report
7. Complete GPX route-package import/export

## Route-leg labels

Each OpenCPN route leg is annotated using exactly:

```text
128°T  0.84 M
```

Requirements:

- true course shown as three digits followed by `°T`
- distance shown with two decimals followed by `M`
- label placed above the route leg when screen space permits
- labels do not alter route geometry
- colour follows the normal route display colour available through OpenCPN plugin API 1.21

## EBL / VRM

### Start point

An EBL/VRM can start at:

- an exact free chart coordinate
- a route waypoint selected by Shift-snap
- a standalone OpenCPN Mark selected by Shift-snap
- a Route TURN start/end point selected by Shift-snap

Graphically moving an existing EBL/VRM start makes it a free chart coordinate and removes its previous snap association.

### Display

The chart overlay contains:

- EBL line
- target ring/crosshair
- compact VRM arc
- optional yellow information note

Default VRM arc width is 30 degrees, centred on the EBL bearing.

### Labels and notes

Requirements:

- stable automatic placement
- manual placement and reset
- manual placement survives restart
- configurable font size
- off-screen target notes are hidden
- all yellow notes can be hidden/viewed globally
- hiding notes leaves EBL/VRM, PIL and TURN geometry visible
- global note visibility survives restart

### Editing

An EBL/VRM must be selectable near its start point, target/end point or the line between them.

Available actions:

- Move start point
- Move target
- Move label
- Reset label position
- Rename
- Delete

## Parallel Index Line

Creation:

1. Select **Route PIL**
2. Select an existing route leg
3. Select the geographic anchor point through which the parallel line should pass

Offset convention:

- positive = SB / starboard
- negative = BB / port

Requirements:

- PIL stays associated with the selected route leg
- multiple PILs may exist on one leg
- graphical editing has a live preview
- PIL uses a compact/tight dashed pattern
- PIL geometry remains visible when yellow notes are globally hidden

## Route TURN

Route TURN operates on an interior route waypoint with inbound and outbound legs.

The user supplies a geometric turn radius. Route Toolbar calculates:

- inbound/outbound true course
- course change and turn side
- tangent distance
- wheel-over / turn-start point
- turn arc
- turn-end point

An optional planned rudder angle can be stored as a manual planning value. TURN data is persistent and included in Route Toolbar GPX package data. TURN start/end points can be used as EBL/VRM Shift-snap starts.

## Route Objects

**Route Objects...** displays saved EBL/VRM, PIL and TURN objects immediately when opened.

The manager supports:

- Show selected
- Hide selected
- Show all
- Hide all
- Delete selected

Show/hide affects chart rendering only; stored objects remain available to GPX/report workflows.

## Persistence

Persisted state includes:

- EBL/VRM objects and visibility
- PIL objects and visibility
- TURN objects and visibility
- route/waypoint/Mark/TURN associations
- EBL/PIL/TURN colours
- label size and manual positions
- global yellow-note visibility

State loading is delayed until OpenCPN navigation objects are available. An unloaded plugin must never overwrite valid persisted state with an empty object set during shutdown.

## Route Report

Outputs:

- PDF A4 landscape
- UTF-8 semicolon-separated CSV

Route data includes waypoint sequence, coordinates, course, leg distance, PIL and EBL/VRM planning information.

## GPX package

Route Toolbar GPX packages use standard GPX 1.1 with plugin data under:

```text
https://nubisor.se/opencpn/route-toolbar/1
```

Package data can include:

- route and waypoint identity
- referenced Marks
- EBL/VRM objects
- PIL geometry
- TURN geometry and planning values
- colours and label settings

Import modes:

- Import as new
- Replace existing

See [GPX_FORMAT.md](GPX_FORMAT.md).

## Context menu

Context-menu registration is deferred until OpenCPN initialization is sufficiently complete. EBL/VRM editing accepts a click close to its start point, target point or line. PIL editing accepts a click near the PIL line.

## Rendering

Supported:

- wxDC overlay rendering
- OpenGL overlay rendering

Both paths must implement object visibility, yellow-note visibility and route-leg labels consistently. macOS OpenGL deprecation warnings are expected during compilation and are not build failures.

## Safety constraints

Route Toolbar must:

- avoid silently changing source OpenCPN route geometry
- preserve persistent planning objects
- avoid duplicate objects during replace-import
- make snap associations explicit
- treat reports and overlays as planning support, not authoritative navigation data
