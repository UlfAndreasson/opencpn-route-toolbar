# Route Toolbar GPX Package Format

## Purpose

A Route Toolbar GPX package is a portable backup/transfer file for a complete
passage-planning route.

The file remains normal GPX 1.1 and can still expose its route and Marks to
software which does not understand Route Toolbar extensions.

## Namespaces

GPX:

```xml
xmlns="http://www.topografix.com/GPX/1/1"
```

Route Toolbar:

```xml
xmlns:rt="https://nubisor.se/opencpn/route-toolbar/1"
```

Current Route Toolbar extension format:

```xml
<rt:route-toolbar version="1">
```

The extension-format version is independent of the plugin application version.

## Standard route

The OpenCPN route is stored as standard GPX:

```xml
<rte>
  <name>BOS-Simulation</name>

  <rtept lat="..." lon="...">
    <name>001</name>
  </rtept>
</rte>
```

## Standalone Marks

Standalone OpenCPN Marks used as EBL/VRM origins are exported as:

```xml
<wpt lat="..." lon="...">
  <name>BOAT</name>
</wpt>
```

## GUID extensions

Route:

```xml
<extensions>
  <rt:route guid="..."/>
</extensions>
```

Waypoint:

```xml
<extensions>
  <rt:waypoint guid="..."/>
</extensions>
```

Mark:

```xml
<extensions>
  <rt:mark guid="..."/>
</extensions>
```

## Route Toolbar package

Example:

```xml
<extensions>
  <rt:route-toolbar
      version="1"
      routeGuid="..."
      routeName="BOS-Simulation">

    <rt:settings .../>
    <rt:ebl-vrm .../>
    <rt:pil .../>
    <rt:turn .../>

  </rt:route-toolbar>
</extensions>
```

## Settings

Example:

```xml
<rt:settings
    labelFontSize="21"
    eblColour="#FB02FF"
    pilColour="#FB0207"
    turnColour="#FF7A00"/>
```

## EBL / VRM

Stored EBL/VRM information includes:

- start coordinates
- target coordinates
- bearing
- range
- VRM arc width
- route GUID/name
- waypoint GUID/name
- standalone Mark GUID/name
- TURN start/end identity where applicable
- target name
- manual-label flag
- manual-label X/Y pixel offset

Route-based EBL/VRM origins are reattached to the corresponding OpenCPN
route/waypoint identity.

Mark-based EBL/VRM origins are reattached by Mark GUID.

## PIL

Stored PIL information includes:

- route GUID/name
- leg index
- from waypoint GUID/name
- to waypoint GUID/name
- offset NM
- anchor coordinates
- leg coordinates
- rendered line coordinates
- course
- leg distance

Offset sign:

```text
positive = SB / starboard
negative = BB / port
```

## TURN

Stored TURN information includes route/waypoint identity, visibility, planned
radius, course-change geometry, turn start/end coordinates and optional planned
rudder angle.

## Import behaviour

OpenCPN remains the owner/editor of routes and waypoints. Route Toolbar treats
the GPX route as the portable route definition, but it does not rewrite an
already existing OpenCPN route merely to restore Route Toolbar planning data.

### Route does not exist

If the package route does not exist in OpenCPN:

- create the route
- create/import its route waypoints
- import EBL/VRM objects
- import PIL objects
- import TURN objects
- retain/reconnect referenced standalone Marks

### Existing route with no Route Toolbar objects

If the package route already exists and has no associated Route Toolbar
objects:

- reuse the existing OpenCPN route unchanged
- do not replace or rewrite its waypoints
- map package waypoint identities onto the existing route
- attach/import EBL/VRM, PIL and TURN objects directly
- do not show a Replace/Import-as-new prompt

### Existing route with Route Toolbar objects

If the route already exists and associated Route Toolbar objects are present,
the user can choose:

**Replace existing**

- keep the existing OpenCPN route and waypoints unchanged
- remove Route Toolbar objects belonging to that package/route
- import the package EBL/VRM, PIL and TURN objects once

**Import as new**

- create a new route identity
- create/remap waypoint identities as needed
- reconnect EBL/VRM objects
- reconnect PIL objects
- reconnect TURN objects
- retain referenced standalone Marks
- avoid duplicate Route Toolbar objects

## Compatibility

Unknown GPX extension namespaces are normally ignored by other GPX-aware
applications.

However, an application which opens and rewrites the file may discard unknown
`<extensions>` elements.

For this reason the original Route Toolbar GPX export should be retained when
the complete passage-planning package is important.