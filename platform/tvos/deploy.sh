#!/bin/bash
# Deploy EZSnes9x to Apple TV
# Usage: ./platform/tvos/deploy.sh [DEVICE_ID]
#
# Prerequisites:
#   1. Configure with SNES_ROMS pointing to your ROM directory:
#      SNES_ROMS=~/snes_games cmake -B build-tvos -DCMAKE_SYSTEM_NAME=tvOS
#
# Optional environment variables:
#   DEVELOPMENT_TEAM - Your Xcode team ID (auto-detected if not set)
#   TVOS_DEVICE_ID   - Device ID (can also be passed as first argument)
#
# Example:
#   SNES_ROMS=~/snes_games cmake -B build-tvos -DCMAKE_SYSTEM_NAME=tvOS
#   ./platform/tvos/deploy.sh 00008110-000A68A00E2B801E

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

# Get device ID from argument or environment
DEVICE_ID="${1:-$TVOS_DEVICE_ID}"
if [ -z "$DEVICE_ID" ]; then
    echo "Error: No device ID specified"
    echo "Usage: $0 [DEVICE_ID]"
    echo ""
    echo "Available devices:"
    xcrun devicectl list devices 2>&1 | grep -E "Apple TV|Living Room" || true
    exit 1
fi

# Auto-detect DEVELOPMENT_TEAM if not set
if [ -z "$DEVELOPMENT_TEAM" ]; then
    DEVELOPMENT_TEAM=$(defaults read com.apple.dt.Xcode "IDEProvisioningTeamByIdentifier" 2>/dev/null | grep teamID | head -1 | sed 's/.*= //;s/;//;s/ //g')
fi

if [ -z "$DEVELOPMENT_TEAM" ]; then
    echo "Error: Could not auto-detect DEVELOPMENT_TEAM"
    echo "Please set it via: DEVELOPMENT_TEAM=XXX $0 $DEVICE_ID"
    echo ""
    echo "To find your team ID, run:"
    echo "  defaults read com.apple.dt.Xcode \"IDEProvisioningTeamByIdentifier\" | grep teamID"
    exit 1
fi

echo "=== Building EZSnes9x for Apple TV ==="
echo "Device ID: $DEVICE_ID"
echo "Team: $DEVELOPMENT_TEAM"
echo ""

# Build the app with proper signing
# Uses -allowProvisioningUpdates to automatically create provisioning profiles
cd "$BUILD_DIR"
xcodebuild \
    -project snes9x.xcodeproj \
    -scheme ezsnes9x-tvos \
    -destination "id=$DEVICE_ID" \
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
    --device "$DEVICE_ID" \
    "$APP_PATH"

echo ""
echo "=== Deployment Complete ==="
echo "EZSnes9x has been installed on your Apple TV!"
