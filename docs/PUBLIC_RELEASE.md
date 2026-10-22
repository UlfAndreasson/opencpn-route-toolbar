# Public release plan — Route Toolbar 0.5.11

This document tracks the path from the current development preview to a public OpenCPN Plugin Manager release.

## Current release state

- Development version: **0.5.11**
- GitHub prerelease target: **v0.5.11-alpha1**
- OpenCPN target: **5.14.2**
- Plugin API: **1.21**
- wxWidgets: **3.2**
- Package targets:
  - macOS universal — arm64 + x86_64
  - Windows x86_64
  - Ubuntu 24.04 x86_64 GTK3

The GitHub release workflow builds OpenCPN Plugin Manager-style tarballs and SHA-256 files for all three package targets. A GitHub prerelease is not the same thing as publication in the official OpenCPN Plugin Manager catalog; catalog metadata must still be tested and submitted to the appropriate OpenCPN catalog channel.

## Positioning

Short description:

> Route-aware coastal passage planning directly on your existing OpenCPN route.

Longer description:

> Plan directly on an existing OpenCPN route with route-aware EBL/VRM, parallel indexing, turn geometry, leg course/distance annotations and passage-plan reports — without recreating the route as separate drawing objects.

Route Toolbar is intended to complement, not replace, general-purpose drawing plugins. Its main distinction is that its planning objects and leg information are tied to the existing OpenCPN route workflow.

## Recommended screenshots

Use these five screenshots for the first public presentation:

1. **Overview** — full route with EBL/VRM, PIL, TURN and route-leg course/distance information visible.
2. **Route Toolbar** — main toolbar window open over the chart.
3. **EBL/VRM editing** — editing controls for start/target/name/delete together with Move labels from the toolbar.
4. **Route Objects** — list of persistent EBL/VRM, PIL and TURN objects with multi-select, show, hide and delete controls.
5. **Route Report CSV** — exported passage-plan data opened in Numbers/Excel-compatible spreadsheet form.

The GPX Replace/Import-as-new dialog is useful for documentation, but is less important as a lead promotional screenshot.

## Release sequence

### 1. GitHub project page

Current README should make the active version visible immediately at the top and keep the development-preview status explicit.

### 2. GitHub prerelease

Current prerelease target:

```text
v0.5.11-alpha1
```

Release title:

```text
Route Toolbar 0.5.11 Alpha 1
```

The release workflow packages macOS universal, Windows x86_64 and Ubuntu 24.04 x86_64 builds and publishes their SHA-256 checksums alongside the tarballs.

### 3. OpenCPN Plugin Manager packaging

The package layout and metadata templates are prepared for the normal OpenCPN plugin distribution workflow.

Before official catalog publication:

- verify each generated package and checksum
- test import/install, enable/disable, update and uninstall through OpenCPN Plugin Manager where supported
- verify plugin version shown by OpenCPN is 0.5.11
- verify macOS universal package on Apple Silicon and Intel
- verify Windows x86_64 package
- verify Ubuntu 24.04 GTK3 x86_64 package
- generate/update catalog XML using the exact release asset URLs and SHA-256 values
- initially submit to the appropriate OpenCPN test/beta catalog channel
- submit to the normal OpenCPN plugins catalog only after platform testing is satisfactory

Do not describe the plugin as generally available through the official Plugin Manager until its catalog metadata has actually been accepted/published.

### 4. Cross-platform test matrix

For each platform verify:

- plugin loads without dependency/runtime errors
- toolbar icon and dialog rendering
- wxDC/OpenGL overlays
- EBL/VRM creation and editing
- PIL creation/editing
- TURN geometry
- Move labels and persistent manual label positions
- no viewport-edge/off-screen label artefacts
- Route Objects persistence and route-order sorting
- route visibility controlling leg info
- PDF/CSV Route Report including TURN/PIL/EBL data
- GPX export/import and existing-route behaviour

### 5. Community announcement

Suggested forum/post title:

```text
Route Toolbar — route-aware EBL/VRM, Parallel Indexing and turn planning for OpenCPN
```

Suggested summary:

> Route Toolbar is an OpenCPN plugin for coastal passage planning and pilotage preparation which works directly with the OpenCPN route you already have. Version 0.5.11 includes route-aware EBL/VRM, parallel index lines, tangent turn geometry, course/distance leg labels, persistent Route Objects, PDF/CSV passage-plan reports and GPX package import/export. The current alpha targets OpenCPN 5.14.2 and is being packaged for macOS universal, Windows x86_64 and Ubuntu 24.04 x86_64.

## What to ask testers

- Does attaching PIL directly to a route leg match the way you plan passages?
- Are EBL/VRM, PIL, TURN and route-leg labels readable and stable while panning/zooming?
- Is the TURN representation useful, or is different turn information needed?
- Is the PDF/CSV route report complete enough for a practical passage plan?
- Does GPX package import/export behave as expected with existing OpenCPN routes?
- Which platform and OpenCPN version are you testing?

## Release principle

Keep OpenCPN as the authoritative owner/editor of routes and waypoints. Route Toolbar should remain a non-destructive planning layer around the route rather than becoming a second route editor.
