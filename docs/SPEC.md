# Route Toolbar — Functional Specification

## Version 0.4.1 scope

Route Toolbar currently covers five primary workflows:

1. Route/Mark based EBL/VRM
2. Route-leg based Parallel Index Lines
3. Route-leg course/distance chart labels
4. PDF/CSV Route Report
5. Complete GPX route-package import/export

## Route-leg labels

Each OpenCPN route leg is annotated with true course and distance using exactly
the compact format:

```text
128°T  0.84 M
```

Requirements:

- true course shown as three digits followed by `°T`
- distance shown with two decimals followed by `M`
- label colour matches the OpenCPN route colour
- default-route colour follows OpenCPN's route display colour
- label is placed above the route leg when screen space permits
- labels do not alter route geometry

## EBL / VRM

### Start point

An EBL/VRM can originate from:

- an OpenCPN route waypoint
- a standalone OpenCPN Mark
- a free chart position after graphical start-point editing

For a route waypoint the plugin stores route and waypoint identity.

For a standalone Mark the plugin stores the Mark GUID and refreshes the origin
from OpenCPN.

If the user graphically moves an existing EBL/VRM start point, the new start is
stored as a free chart coordinate and the previous waypoint/Mark association is
removed.

### Target

Stored target information:

- latitude
- longitude
- name
- true bearing
- range NM

### Display

The chart overlay contains:

- EBL line
- target ring/crosshair
- compact VRM arc
- yellow information label

Default VRM arc:

- width: 30 degrees
- orientation: centred on the EBL bearing

### Labels

Requirements:

- stable positioning between redraws
- no frame-to-frame dynamic reshuffling
- off-screen target labels are hidden
- manual positioning is supported
- manual position survives restart
- manual position can be reset
- configurable font size
- all yellow notes can be hidden/viewed globally from Route Toolbar
- hiding notes does not hide the EBL/VRM or PIL geometry
- global note visibility survives restart

### Editing

An existing EBL/VRM must be selectable for editing by clicking near:

- its start point
- its target/end point
- the EBL line between them

The chart context menu uses the same geometry hit test, so the target
crosshair/arc is not the only valid edit location.

EBL/VRM actions:

- Move start point
- Move target
- Move label
- Reset label position
- Rename
- Delete

## Parallel Index Line

### Creation

1. Select Route PIL
2. Select existing route leg
3. Select geographic anchor point

The existing route leg is the reference centreline.

### Offset convention

- positive offset = SB / starboard
- negative offset = BB / port

### Behaviour

- PIL remains tied to the selected route leg
- editing must not recenter it on Own Ship
- multiple PIL objects may exist on one leg
- graphical editing uses a live preview
- original PIL should not remain duplicated while moving

### Stored data

- route GUID/name
- leg index
- from/to waypoint GUID/name
- route leg endpoints
- leg course
- leg distance
- anchor position
- signed offset NM
- rendered line endpoints

## Persistence

Persisted information includes:

- EBL/VRM objects
- PIL objects
- route/waypoint associations
- standalone Mark associations
- colours
- label size
- manual label positions
- yellow-note visibility

State loading is delayed until OpenCPN navigation objects are available.

An unloaded plugin must never overwrite valid persisted state with an empty
object set during shutdown.

## Route Report

Output:

- PDF A4 landscape
- UTF-8 semicolon-separated CSV

Route-level information:

- route name
- export timestamp
- total route distance

Waypoint/leg information:

- waypoint sequence
- waypoint name
- waypoint coordinates
- course to next waypoint
- leg distance
- zero or more PIL entries
- zero or more EBL/VRM entries

PIL format:

```text
SB: 0.02 NM
BB: 0.03 NM
```

EBL/VRM fields:

- sight-mark name
- bearing °T
- range NM
- sight-mark latitude
- sight-mark longitude

Mark-origin EBL/VRM objects are exported in a separate report section.

## GPX package

Route Toolbar GPX packages use standard GPX 1.1.

Standard objects:

- `<rte>`
- `<rtept>`
- referenced `<wpt>`

Plugin data uses:

```text
https://nubisor.se/opencpn/route-toolbar/1
```

under GPX `<extensions>`.

Import modes:

- Import as new
- Replace existing

Route-based EBL/VRM imports must resolve the imported waypoint GUID and use the
exact OpenCPN waypoint coordinate as origin.

Mark-based EBL/VRM imports must resolve the Mark GUID.

See [GPX_FORMAT.md](GPX_FORMAT.md).

## Context menu

Chart context-menu registration is deferred until OpenCPN initialization has
completed sufficiently for navigation objects and UI hooks to be safe.

For Route Toolbar EBL/VRM objects, context editing must accept a click close to
the start point, target point or EBL line.

## Rendering

Supported:

- wxDC overlay rendering
- OpenGL overlay rendering

Both rendering paths must implement route-leg labels and yellow-note visibility
consistently.

macOS OpenGL deprecation warnings are expected during compilation and are not
build failures.

## Safety constraints

Route Toolbar must:

- preserve route/waypoint/Mark relationships until the user explicitly moves a linked EBL start
- avoid duplicate planning objects during replace-import
- avoid silently changing source route geometry
- preserve valid persistent state
- treat reports as planning output, not authoritative navigation data
