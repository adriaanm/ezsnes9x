#!/bin/bash
# Deploy EZSnes9x to Apple TV
#
# Prerequisites:
#   Configure with SNES_ROMS pointing to your ROM directory:
#     SNES_ROMS=~/snes_games cmake -B build-tvos -DCMAKE_SYSTEM_NAME=tvOS
#
# Usage:
#   ./platform/tvos/deploy.sh          # auto-detect single paired Apple TV
#
# Optional environment variable:
#   DEVELOPMENT_TEAM - Your Xcode team ID (auto-detected if not set)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-tvos"

# Check if CMake was configured
if [ ! -d "$BUILD_DIR/snes9x.xcodeproj" ]; then
    echo "Error: Build directory not configured"
    echo "Please run: SNES_ROMS=~/snes_games cmake -B build-tvos -DCMAKE_SYSTEM_NAME=tvOS"
    exit 1
fi

# Find exactly one paired Apple TV via devicectl.
# Extracts the device UDID (xcodebuild-compatible, 8-16 hex format) from
# potentialHostnames, which is distinct from the CoreDevice UUID that
# 'devicectl list devices' displays in its text output.
DEVICE_JSON=$(mktemp /tmp/ezsnes9x-devices-XXXXXX.json)
trap "rm -f $DEVICE_JSON" EXIT

xcrun devicectl list devices --json-output "$DEVICE_JSON" >/dev/null 2>&1

DEVICE_INFO=$(python3 - "$DEVICE_JSON" << 'PYEOF'
import json, sys, re

data = json.load(open(sys.argv[1]))
devices = data.get("result", {}).get("devices", [])

paired_tvs = [
    d for d in devices
    if d.get("hardwareProperties", {}).get("deviceType") == "appleTV"
    and d.get("connectionProperties", {}).get("pairingState") == "paired"
]

if len(paired_tvs) == 0:
    print("Error: No paired Apple TV found.", file=sys.stderr)
    print("Make sure your Apple TV is on and has been paired via Xcode.", file=sys.stderr)
    sys.exit(1)

if len(paired_tvs) > 1:
    names = [d.get("deviceProperties", {}).get("name", "Unknown") for d in paired_tvs]
    print(f"Error: {len(paired_tvs)} Apple TVs paired: {', '.join(names)}", file=sys.stderr)
    print("Turn off all but one before deploying.", file=sys.stderr)
    sys.exit(1)

tv = paired_tvs[0]
name = tv.get("deviceProperties", {}).get("name", "Unknown")
hostnames = tv.get("connectionProperties", {}).get("potentialHostnames", [])

# The UDID for xcodebuild has the form XXXXXXXX-XXXXXXXXXXXXXXXX (8-16 hex).
# The CoreDevice UUID shown by 'devicectl list devices' is a standard UUID
# (8-4-4-4-12) and does NOT work with xcodebuild -destination id=...
udid = None
for h in hostnames:
    m = re.match(r'^([0-9A-Fa-f]{8}-[0-9A-Fa-f]{16})\.coredevice\.local$', h)
    if m:
        udid = m.group(1)
        break

if not udid:
    print("Error: Could not find device UDID in devicectl output.", file=sys.stderr)
    sys.exit(1)

print(f"{udid}\t{name}")
PYEOF
) || exit 1

XCODE_DEVICE_ID=$(echo "$DEVICE_INFO" | cut -f1)
DEVICE_NAME=$(echo "$DEVICE_INFO" | cut -f2)

# Auto-detect DEVELOPMENT_TEAM if not set
if [ -z "$DEVELOPMENT_TEAM" ]; then
    DEVELOPMENT_TEAM=$(defaults read com.apple.dt.Xcode "IDEProvisioningTeamByIdentifier" 2>/dev/null | grep teamID | head -1 | sed 's/.*= //;s/;//;s/ //g')
fi

if [ -z "$DEVELOPMENT_TEAM" ]; then
    echo "Error: Could not auto-detect DEVELOPMENT_TEAM"
    echo "Please set it via: DEVELOPMENT_TEAM=XXX ./platform/tvos/deploy.sh"
    echo ""
    echo "To find your team ID, run:"
    echo "  defaults read com.apple.dt.Xcode \"IDEProvisioningTeamByIdentifier\" | grep teamID"
    exit 1
fi

echo "=== Building EZSnes9x for Apple TV ==="
echo "Device:      $DEVICE_NAME ($XCODE_DEVICE_ID)"
echo "Team:        $DEVELOPMENT_TEAM"
echo ""

cd "$BUILD_DIR"
xcodebuild \
    -project snes9x.xcodeproj \
    -scheme ezsnes9x-tvos \
    -destination "id=$XCODE_DEVICE_ID" \
    -configuration Release \
    -allowProvisioningUpdates \
    -allowProvisioningDeviceRegistration \
    CODE_SIGN_STYLE=Automatic \
    DEVELOPMENT_TEAM="$DEVELOPMENT_TEAM" \
    PRODUCT_BUNDLE_IDENTIFIER=com.ezsnes9x.tvos

APP_PATH="$BUILD_DIR/platform/tvos/Release-appletvos/EZSnes9x.app"

if [ ! -d "$APP_PATH" ]; then
    echo "Error: Built app not found at $APP_PATH"
    exit 1
fi

echo ""
echo "=== Installing on Apple TV ==="

xcrun devicectl device install app \
    --device "$XCODE_DEVICE_ID" \
    "$APP_PATH"

echo ""
echo "=== Done ==="
echo "EZSnes9x installed on $DEVICE_NAME"
