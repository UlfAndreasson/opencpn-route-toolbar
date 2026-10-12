#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-0.5.9}"
PLUGIN="${2:-build/libroute_toolbar_pi.dylib}"
OUTDIR="${3:-dist}"

if [[ ! -f "$PLUGIN" ]]; then
  echo "Plugin binary not found: $PLUGIN" >&2
  exit 1
fi

ROOT="route_toolbar_pi-${VERSION}_darwin-wx32-10-arm64"
STAGE="$OUTDIR/$ROOT"
TARBALL="$OUTDIR/${ROOT}.tar.gz"
CATALOG="$OUTDIR/RouteToolbar-${VERSION}-darwin-wx32-arm64-10.xml"

rm -rf "$STAGE"
mkdir -p "$STAGE/OpenCPN.app/Contents/PlugIns"

cp "$PLUGIN" "$STAGE/OpenCPN.app/Contents/PlugIns/libroute_toolbar_pi.dylib"

sed "s/@VERSION@/${VERSION}/g" \
  packaging/macos/package-metadata.xml.in \
  > "$STAGE/metadata.xml"

mkdir -p "$OUTDIR"
rm -f "$TARBALL"
tar -C "$OUTDIR" -czf "$TARBALL" "$ROOT"

CHECKSUM=$(shasum -a 256 "$TARBALL" | awk '{print $1}')
ASSET=$(basename "$TARBALL")
TARBALL_URL="https://github.com/UlfAndreasson/opencpn-route-toolbar/releases/download/v${VERSION}/${ASSET}"

sed \
  -e "s/@VERSION@/${VERSION}/g" \
  -e "s|@TARBALL_URL@|${TARBALL_URL}|g" \
  -e "s/@TARBALL_CHECKSUM@/${CHECKSUM}/g" \
  packaging/macos/catalog-metadata.xml.in \
  > "$CATALOG"

echo "Created:"
echo "  $TARBALL"
echo "  $CATALOG"
echo "SHA256: $CHECKSUM"
echo
echo "Tarball contents:"
tar -tzf "$TARBALL"
