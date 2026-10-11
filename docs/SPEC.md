# Route Toolbar — Functional Specification

## Version 0.4.0 scope

Route Toolbar currently covers four primary workflows:

1. Route/Mark based EBL/VRM
2. Route-leg based Parallel Index Lines
3. PDF/CSV Route Report
4. Complete GPX route-package import/export

## EBL / VRM

### Start point

An EBL/VRM can originate from:

- an OpenCPN route waypoint
- a standalone OpenCPN Mark

For a route waypoint the plugin stores route and waypoint identity.

For a standalone Mark the plugin stores the Mark GUID and refreshes the origin
from OpenCPN.

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

### Editing

EBL/VRM actions:

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

## Rendering

Supported:

- wxDC overlay rendering
- OpenGL overlay rendering

macOS OpenGL deprecation warnings are expected during compilation and are not
build failures.

## Safety constraints

Route Toolbar must:

- preserve route/waypoint/Mark relationships
- avoid duplicate planning objects during replace-import
- avoid silently changing source route geometry
- preserve valid persistent state
- treat reports as planning output, not authoritative navigation data
