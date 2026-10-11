# OpenCPN Route Toolbar

A route/passage-planning plugin for OpenCPN focused on practical pilotage tools which are easier to use than the current OCPN Draw workflow.

## Planned tools

### Route EBL/VRM
- Pick a point on/near a route or waypoint.
- Pick a visual/radar target such as a lighthouse, buoy, beacon or conspicuous object.
- Draw an EBL from the route point to the target.
- Draw a configurable VRM **arc** instead of a full circle (default: 30° / ±15° around the EBL bearing).
- Show bearing and range clearly on the chart.
- Store target name/description and target position.
- Associate the object with a route and waypoint/leg.

### Route PIL
- Attach a Parallel Index Line directly to an existing route leg.
- Use the route leg itself as the reference centreline; no duplicate manual centreline placement.
- Configure port/starboard offsets independently.
- Changing an offset must never move the PIL back to Own Ship.
- Associate PIL data with the route leg.

### Route Report
Export a route/passage plan combining OpenCPN route data with Route Toolbar planning objects.

Initial export formats:
- CSV (Excel / Numbers / Google Sheets)
- HTML printable report

Planned report data includes:
- Route name and total distance
- Waypoint number/name/description and position
- Course and leg distance
- EBL target / sight mark
- Bearing and range
- VRM arc settings
- PIL port/starboard offsets
- Notes

Future candidate:
- Turning radius / Wheel-Over-Point (WOP) planning

## Object management

Where practical, Route Toolbar should integrate with or provide equivalent functionality to OpenCPN/OCPN Draw's Path & Point Manager so that EBL/VRM and PIL objects can be shown, hidden, edited, exported and deleted centrally.

## Design goals

1. **Route-aware** — planning objects belong to a route, waypoint or route leg.
2. **Few clicks** — avoid manually recreating route geometry.
3. **Clear chart display** — short arcs instead of large overlapping VRM circles.
4. **Persistent** — objects survive OpenCPN restart reliably.
5. **Reportable** — route + pilotage information can be exported as one coherent passage plan.
6. **Non-destructive** — never modify an OpenCPN route unexpectedly.

## Current status

### v0.2.2 Route EBL/VRM prototype

The plugin targets OpenCPN 5.14.2 / API 1.21 and includes a working Route EBL/VRM workflow:

- **Route Toolbar** opens as a non-modal OpenCPN tool window.
- Select **Route EBL / VRM**.
- Click near a route waypoint; the click snaps to the nearest waypoint within 0.10 NM.
- Click the lighthouse, buoy, beacon or other sight mark.
- The selected geometry is frozen while the target-name dialog is open, so the preview does not follow the mouse onto the dialog.
- Enter a target/sight-mark name.
- The chart shows:
  - EBL line from the route waypoint to the target
  - true bearing from route point to target
  - range in NM
  - a compact 30° VRM arc centred on the EBL bearing instead of a full circle
- While choosing the target, the EBL and VRM arc are previewed live.
- Repeated EBL/VRM objects can be created in one session; right-click exits creation mode.
- Both standard and OpenGL chart canvases are supported.

Current limitations of this prototype:

- first point snaps to route **waypoints** only; arbitrary points on a route leg come next
- EBL/VRM objects are currently in-memory only and do not yet survive restart
- manager integration / edit / show-hide / delete / export is not implemented yet
- Route PIL and Route Report are still placeholders

See [docs/BUILDING.md](docs/BUILDING.md) and [docs/SPEC.md](docs/SPEC.md).

## Upstream references

- OpenCPN: https://github.com/OpenCPN/OpenCPN
- OCPN Draw: https://github.com/jongough/ocpn_draw_pi
- OpenCPN plugin API libraries: https://github.com/OpenCPN/opencpn-libs

OCPN Draw is an important implementation reference for EBL, PIL, drawing, persistence and GPX handling. If Route Toolbar incorporates GPL-licensed OCPN Draw code, Route Toolbar will be distributed under a GPL-compatible license and retain the required attribution/license notices.
