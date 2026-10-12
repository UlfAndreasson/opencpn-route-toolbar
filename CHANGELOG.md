# Changelog

## 0.5.5 — 2026-10-12

### Changed

- route-leg course/distance labels use one consistent reading direction: rotate the screen left to read them
- leg labels stay on the same side of the route line instead of alternating sides
- Route Report uses action buttons for PDF/CSV instead of a choice field
- Route GPX uses action buttons for export/import instead of a choice field
- the visibility button is now labelled Hide/View notes and leg info

## 0.5.4 — 2026-10-12

### Changed

- route-leg course/distance text is rotated to follow the exact route-leg angle
- 090/270 degree legs remain horizontal, 000/180 degree legs become vertical, and diagonal legs follow their line angle
- labels remain centred near the leg midpoint with approximately 1 px clearance from the route line
- OpenGL and wxDC rendering use the same rotated placement

## 0.5.3 — 2026-10-12

### Changed

- route-leg course/distance labels now sit essentially on the route line with a 1 px clearance
- mostly vertical legs keep the label immediately to the left of the line

## 0.5.2 — 2026-10-12

### Changed

- route-leg course/distance labels are anchored to the midpoint of each leg
- mostly horizontal legs place the label immediately above the route line
- mostly vertical legs place the label immediately to the left of the route line
- route-leg labels no longer clamp to viewport edges, preventing apparent label movement while panning
- route-leg labels now follow the global **Hide/View all yellow notes** control

## 0.5.1 — 2026-10-12

### Fixed

- Route TURN yellow notes now follow the global **Hide/View all yellow notes** control
- starting a new Route EBL/VRM no longer opens the edit menu when clicking near existing EBL geometry
- Route Toolbar is explicitly restored and repainted after closing the EBL/VRM edit action dialog on macOS, preventing the blank/grey toolbar state before graphical move operations

## 0.5.0 — 2026-10-12

### Added

- Route TURN geometry at interior route waypoints
- planned TURN radius and optional manual rudder-angle planning value
- TURN persistence and GPX package support
- Route Objects manager for EBL/VRM, PIL and TURN objects
- TURN start/end snap points for EBL/VRM creation

### Fixed

- Route Objects now populates immediately when opened
- Route TURN / Route Objects development is integrated with the 0.4.1 route-label and EBL editing changes

### Changed

- PIL uses a tighter dashed pattern
- global **Hide all yellow notes / View all yellow notes** works with the integrated 0.5.0 tools while leaving planning geometry visible
- plugin version advanced to 0.5.0

## 0.4.1 — 2026-10-12

### Added

- route-leg chart labels using compact true-course/distance format such as `128°T  0.84 M`
- route-leg labels prefer placement above the leg
- EBL/VRM start-point graphical editing
- EBL/VRM selection/editing from start point, target point or EBL line
- global **Hide all yellow notes / View all yellow notes** control in Route Toolbar
- persisted yellow-note visibility setting

### Changed

- hiding yellow notes leaves EBL/VRM and PIL geometry visible for a cleaner route-planning view
- moving an EBL/VRM start point graphically detaches that start from its previous snap association

## 0.4.0 — 2026-10-12

First feature-complete Route Toolbar development milestone.

### Added

- Route-aware EBL/VRM
- OpenCPN Mark-based EBL/VRM
- compact VRM arc
- configurable EBL label size
- configurable EBL/VRM colour
- configurable PIL colour
- route-leg based PIL
- multiple PIL offsets per leg
- graphical PIL editing
- chart context-menu editing
- EBL target editing
- manual EBL label placement
- resettable label placement
- delayed persistent-state loading
- PDF Route Report
- CSV Route Report
- complete GPX route-package export/import

### Compatibility

- OpenCPN 5.14.2
- Plugin API 1.21
- wxWidgets 3.2
- macOS Apple Silicon development target
