# Public release plan — Route Toolbar 0.5.8

This document tracks the path from the current development preview to a public OpenCPN Plugin Manager release.

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
3. **EBL/VRM editing** — action-button menu showing Move start point / Move target / Move label / Rename / Delete.
4. **Route Objects** — list of persistent EBL/VRM, PIL and TURN objects with Select all / Show / Hide / Delete controls.
5. **Route Report CSV** — exported passage-plan data opened in Numbers/Excel-compatible spreadsheet form.

The GPX Replace/Import-as-new dialog is useful for documentation, but is less important as a lead promotional screenshot.

## Release sequence

### 1. GitHub project page

- keep README focused on the user problem and route-aware workflow
- add the overview screenshot near the top
- add the Route Toolbar, EBL edit, Route Objects and report screenshots further down
- make current platform support and development-preview status explicit

### 2. GitHub release

Create tag/release:

```text
v0.5.8
```

Suggested title:

```text
Route Toolbar 0.5.8 — first public development preview
```

Suggested release notes:

> Route Toolbar is an OpenCPN plugin for route-aware coastal passage planning and pilotage preparation.
>
> 0.5.8 provides EBL/VRM sight lines, parallel index lines attached to route legs, turn geometry at route waypoints, automatic route-leg course/distance labels, persistent Route Objects, PDF/CSV reports and complete GPX route-package import/export.
>
> This first public preview currently targets OpenCPN 5.14.2 on macOS Apple Silicon. Wider platform packaging and OpenCPN Plugin Manager integration are the next steps.
>
> Feedback and testing are especially welcome around real-world pilotage workflows, UI clarity and cross-platform behaviour.

### 3. OpenCPN Plugin Manager packaging

Target the normal OpenCPN plugin distribution workflow rather than asking ordinary users to build from source.

Work required:

- adapt/build the project for OpenCPN Plugin Manager packaging
- create platform-specific plugin tarballs
- generate plugin metadata
- add CI builds for the target platforms
- test install/uninstall/update through OpenCPN Plugin Manager
- initially publish through the appropriate test/beta channel
- submit metadata to the OpenCPN plugins catalog repository

Do not promote the plugin as generally installable through Plugin Manager until these packages have been tested.

### 4. Cross-platform test matrix

Priority order:

1. macOS Apple Silicon — existing development platform
2. macOS Intel/universal compatibility
3. Windows x64
4. Linux x86_64

For each platform verify:

- plugin loads without dependency/runtime errors
- toolbar icon and dialog rendering
- wxDC/OpenGL overlays
- EBL/VRM creation and editing
- PIL creation/editing
- TURN geometry
- Route Objects persistence
- route visibility controlling leg info
- PDF/CSV report output
- GPX export/import and existing-route behaviour

### 5. Community announcement

Suggested forum/post title:

```text
Route Toolbar — route-aware EBL/VRM, Parallel Indexing and turn planning for OpenCPN
```

Draft announcement:

> I have been developing a new OpenCPN plugin called Route Toolbar, aimed at coastal passage planning and pilotage preparation.
>
> The idea is simple: instead of recreating an OpenCPN route using separate drawing objects, Route Toolbar works directly with the route you already have.
>
> Current features include:
>
> - route-aware EBL/VRM sight lines
> - Parallel Index Lines attached directly to route legs
> - turn-radius geometry at route waypoints
> - automatic true-course/distance labels on route legs
> - persistent/editable planning objects
> - PDF and CSV passage-plan reports
> - complete GPX route-package export/import
>
> The current 0.5.8 development preview is working on OpenCPN 5.14.2 on macOS Apple Silicon. I am now preparing proper Plugin Manager packaging and would be interested in feedback from OpenCPN users — particularly anyone using OpenCPN for coastal pilotage, commercial training or passage planning.
>
> Screenshots and source: [GitHub repository]

## What to ask testers

Avoid a generic “what do you think?” request. Ask concrete questions:

- Does attaching PIL directly to a route leg match the way you plan passages?
- Are the EBL/VRM labels and route-leg annotations readable at practical chart scales?
- Is the TURN representation useful, or is different turn information needed?
- What data is missing from the PDF/CSV route report?
- Are there workflows in Ocpn_Draw or other plugins that Route Toolbar should interoperate with rather than duplicate?
- Which platform/OpenCPN version are you testing?

## Release principle

Keep OpenCPN as the authoritative owner/editor of routes and waypoints. Route Toolbar should remain a non-destructive planning layer around the route rather than becoming a second route editor.
