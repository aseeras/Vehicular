#!/usr/bin/env bash
# Packages Vehicular Manslaughter for macOS and wraps the .app in a .dmg.
#
# Usage (from the repo root, on a Mac with Unreal Engine and Xcode installed):
#   ./Scripts/build_mac_dmg.sh                       # auto-detects the newest UE install
#   UE_ROOT="/Users/Shared/Epic Games/UE_5.7" ./Scripts/build_mac_dmg.sh
#   CONFIG=Development ./Scripts/build_mac_dmg.sh    # default is Shipping
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT="$REPO_ROOT/Vehicular.uproject"
CONFIG="${CONFIG:-Shipping}"
OUT_DIR="$REPO_ROOT/Build/MacPackage"
DMG_PATH="$REPO_ROOT/Build/VehicularManslaughter-Mac.dmg"
APP_TITLE="Vehicular Manslaughter"

if [[ "$(uname)" != "Darwin" ]]; then
	echo "This script must run on macOS." >&2
	exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
	echo "Xcode command line tools not found. Install Xcode from the App Store, then run: xcode-select --install" >&2
	exit 1
fi

# Find Unreal Engine: $UE_ROOT, else the newest UE_5.x in the default Epic location.
if [[ -z "${UE_ROOT:-}" ]]; then
	UE_ROOT="$(ls -d "/Users/Shared/Epic Games"/UE_5.* 2>/dev/null | sort -V | tail -n 1 || true)"
fi
UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
if [[ -z "$UE_ROOT" || ! -x "$UAT" ]]; then
	echo "Unreal Engine not found. Set UE_ROOT, e.g.:" >&2
	echo "  UE_ROOT=\"/Users/Shared/Epic Games/UE_5.7\" $0" >&2
	exit 1
fi
echo "Using Unreal Engine at: $UE_ROOT"

if [[ ! -d "$REPO_ROOT/Content/Vehicles" ]]; then
	echo "WARNING: Content/Vehicles not found. Cars will have no mesh in the packaged game."
	echo "         In the editor: Add > Add Feature or Content Pack > Vehicle, then re-run."
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

echo "Building, cooking and packaging ($CONFIG)... this takes a while the first time."
"$UAT" BuildCookRun \
	-project="$PROJECT" \
	-noP4 -utf8output -unattended \
	-platform=Mac \
	-clientconfig="$CONFIG" \
	-build -cook -allmaps -stage -pak -prereqs \
	-archive -archivedirectory="$OUT_DIR"

APP="$(find "$OUT_DIR" -maxdepth 3 -name "*.app" -type d | head -n 1)"
if [[ -z "$APP" ]]; then
	echo "Packaging finished but no .app was found under $OUT_DIR" >&2
	exit 1
fi
echo "Packaged app: $APP"

# Lay out the disk image: the app plus an Applications shortcut for drag-to-install.
STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT
cp -R "$APP" "$STAGING/$APP_TITLE.app"
ln -s /Applications "$STAGING/Applications"

# The build is unsigned; ad-hoc sign so it at least launches locally.
codesign --force --deep --sign - "$STAGING/$APP_TITLE.app" || echo "Ad-hoc signing failed; the app may still run via right-click > Open."

rm -f "$DMG_PATH"
hdiutil create -volname "$APP_TITLE" -srcfolder "$STAGING" -ov -format UDZO "$DMG_PATH"

echo
echo "Done: $DMG_PATH"
echo "First launch on another Mac: right-click the app > Open (it isn't notarized)."
