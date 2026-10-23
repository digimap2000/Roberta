#!/usr/bin/env bash
# Builds "Roberta Control.app" next to this package so it can be launched from
# Finder or the Dock. Needs the software/ release preset built first (it links
# libroberta_desktop_ffi from build/software/release/core).
set -euo pipefail

package_dir="$(cd "$(dirname "$0")/.." && pwd)"
app="$package_dir/Roberta Control.app"

swift build --package-path "$package_dir" -c release
binary="$(swift build --package-path "$package_dir" -c release --show-bin-path)/RobertaControl"

rm -rf "$app"
mkdir -p "$app/Contents/MacOS"
cp "$binary" "$app/Contents/MacOS/RobertaControl"
cp "$package_dir/Support/Info.plist" "$app/Contents/Info.plist"
codesign --force --sign - "$app" >/dev/null

echo "Built $app"
