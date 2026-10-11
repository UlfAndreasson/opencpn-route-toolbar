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

  </rt:route-toolbar>
</extensions>
```

## Settings

Example:

```xml
<rt:settings
    labelFontSize="21"
    eblColour="#FB02FF"
    pilColour="#FB0207"/>
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
- target name
- manual-label flag
- manual-label X/Y pixel offset

Route-based EBL/VRM origins are reattached to the exact imported OpenCPN
waypoint.

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

## Import as new

Import as new:

- creates a new route identity
- remaps waypoint identities where needed
- reconnects EBL/VRM objects
- reconnects PIL objects
- retains referenced standalone Marks
- avoids creating duplicate Route Toolbar objects

## Replace existing

Replace existing:

- identifies the existing route package using GUID
- replaces route/package state
- removes Route Toolbar objects belonging to the replaced package
- imports the package data once

## Compatibility

Unknown GPX extension namespaces are normally ignored by other GPX-aware
applications.

However, an application which opens and rewrites the file may discard unknown
`<extensions>` elements.

For this reason the original Route Toolbar GPX export should be retained when
the complete passage-planning package is important.
