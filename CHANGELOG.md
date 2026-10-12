# Changelog

## 0.4.1 — 2026-10-12

### Added

- route-leg chart labels using compact true-course/distance format such as `128°T  0.84 M`
- route-leg labels prefer placement above the leg and use the matching OpenCPN route colour
- EBL/VRM start-point graphical editing
- EBL/VRM selection/editing from start point, target point or EBL line
- global **Hide all yellow notes / View all yellow notes** control in Route Toolbar
- persisted yellow-note visibility setting

### Changed

- hiding yellow notes now leaves EBL/VRM and PIL geometry visible for a cleaner route-planning view
- moving an EBL/VRM start point graphically detaches that start from its previous route waypoint/Mark association

## 0.4.0 — 2026-10-12

First feature-complete Route Toolbar development milestone.

### Added

- Route-aware EBL/VRM
- OpenCPN Mark-based EBL/VRM
- Mark tracking using OpenCPN GUID
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
- report section for Mark-based EBL/VRM
- separate SB/BB PIL report rows
- complete GPX route-package export
- complete GPX route-package import
- Import as new
- Replace existing
- route/waypoint/Mark GUID preservation

### Changed

- off-screen EBL target labels are hidden instead of pinned to chart edges
- label placement is stable instead of dynamically reshuffled each redraw
- PIL report output uses `SB:` / `BB:` notation

### Compatibility

- OpenCPN 5.14.2
- Plugin API 1.21
- wxWidgets 3.2
- macOS Apple Silicon development target
