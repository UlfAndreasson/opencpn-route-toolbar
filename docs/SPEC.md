# Route Toolbar — Initial Functional Specification

## Scope for v0.1

The first usable version should solve three concrete passage-planning workflows:

1. Create route-aware EBL/VRM objects with a short VRM arc.
2. Create route-aware PIL offsets directly from route legs.
3. Export route + pilotage data as one report.

## Route EBL/VRM

### Interaction
1. Activate **Route EBL/VRM** from the Route Toolbar.
2. Click a route waypoint or a point on a route leg.
3. Click the target/sight mark.
4. Enter or edit target name/description.
5. Object is stored and remains editable after restart.

### Display
- EBL line from route point to target.
- Default VRM arc: 90° total, centred on the EBL bearing (±45°).
- Arc radius equals target range.
- Optional configurable arc width and orientation later.
- Bearing and range displayed near the object.
- EBL/VRM information font should be configurable independently from hover tooltip font.

### Stored data
- UUID
- Route GUID/name
- Route waypoint GUID/name when applicable
- Route leg start/end GUID when applicable
- Origin latitude/longitude
- Target latitude/longitude
- Target name / description
- True bearing
- Range in NM
- Arc start/end angle or arc width
- Visible/hidden
- Persistent state
- Style settings

## Route PIL

### Interaction
1. Activate **Route PIL**.
2. Click an existing route leg.
3. Plugin reads the leg course directly.
4. Set port and/or starboard offsets.
5. PIL remains geographically fixed to that route leg until explicitly changed.

### Behaviour
- No separate manually positioned centreline required.
- Updating an offset must not re-centre the PIL on Own Ship.
- Route geometry is reference-only; Route Toolbar must not silently alter the route.
- Multiple offsets per side may be supported later, but v0.1 requires one port and one starboard offset.

### Stored data
- UUID
- Route GUID/name
- Route leg start/end GUID
- Leg course
- Port offset NM (optional)
- Starboard offset NM (optional)
- Visible/hidden
- Persistent state
- Style settings

## Route Report

### v0.1 output
- CSV
- Printable HTML

### Route-level data
- Route name
- Total distance
- Export timestamp

### Per waypoint / leg
- Waypoint sequence
- Waypoint name
- Description
- Latitude / longitude
- Course to next waypoint
- Leg distance
- EBL/VRM target 1..n
  - target name
  - target position
  - bearing true
  - range NM
  - arc width
- PIL
  - port offset NM
  - starboard offset NM
- Notes

The report should not assume exactly two EBL targets. The UI may optimise for two common sight marks, while the data model supports 0..n targets per waypoint/leg.

## Manager

Preferred outcome: Route Toolbar objects should be manageable in one central manager with:
- show/hide
- properties/edit
- delete
- centre view
- export
- filtering by route

Re-use/integration with OCPN Draw Path & Point Manager should be investigated before creating a separate manager.

## Persistence

Persistence must be deterministic across normal OpenCPN restarts. Route Toolbar should not rely on transient state such as Own Ship position for fixed route-planning objects.

## Future work

### Turning / WOP
- Configurable turning radius
- Calculate Wheel-Over Point from incoming/outgoing legs
- Draw turn arc
- Flag overlapping/impossible turns
- Include WOP/turn data in Route Report

### Additional report fields
- Magnetic bearing/course
- Safety depth / UKC notes
- Tidal/current notes
- ETA/ETD
- Remarks / contingency notes
