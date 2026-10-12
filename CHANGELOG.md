# Changelog

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
