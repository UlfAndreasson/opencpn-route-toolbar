#!/bin/bash
set -euo pipefail

plugin="${1:?usage: fix-macos-libs.sh /path/to/plugin.dylib}"
runtime_path='@executable_path/../Frameworks'

while IFS= read -r lib; do
  [ -n "$lib" ] || continue
  base="$(basename "$lib")"
  install_name_tool -change "$lib" "$runtime_path/$base" "$plugin"
done < <(otool -L "$plugin" | awk '/libwx/ {print $1}')

codesign --force --sign - "$plugin"

echo "wxWidgets dependencies after rewrite:"
otool -L "$plugin" | grep wx || true
