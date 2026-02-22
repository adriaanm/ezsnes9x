#!/bin/bash
# Copy SNES ROMs and cover art to Apple TV
# Usage: ./copy-roms-to-appletv.sh <device-id> <source-directory>

set -e  # Exit on error

DEVICE_ID="${1}"
SOURCE_DIR="${2}"
BUNDLE_ID="com.ezsnes9x.tvos"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_error() {
    echo -e "${RED}ERROR: $1${NC}" >&2
}

print_success() {
    echo -e "${GREEN}$1${NC}"
}

print_info() {
    echo -e "${YELLOW}$1${NC}"
}

# Check arguments
if [ -z "$DEVICE_ID" ] || [ -z "$SOURCE_DIR" ]; then
    print_error "Usage: $0 <device-id> <source-directory>"
    echo ""
    echo "Example: $0 00008110-000A68A00E2B801E ~/snes_games"
    echo ""
    echo "Available devices:"
    xcrun devicectl list devices 2>&1 | grep -E "Apple TV|Living Room|id:" || echo "  No devices found"
    exit 1
fi

# Check if source directory exists
if [ ! -d "$SOURCE_DIR" ]; then
    print_error "Source directory does not exist: $SOURCE_DIR"
    exit 1
fi

# Check if device is connected
print_info "Checking device connection..."
if ! xcrun devicectl list devices 2>&1 | grep -q "$DEVICE_ID"; then
    print_error "Device $DEVICE_ID not found"
    echo ""
    echo "Available devices:"
    xcrun devicectl list devices 2>&1 | grep -E "Apple TV|Living Room|id:"
    exit 1
fi

print_success "Device connected: $DEVICE_ID"

# Count files to copy
cd "$SOURCE_DIR" || exit 1
ROM_COUNT=$(ls -1 *.sfc *.smc 2>/dev/null | wc -l | tr -d ' ')
PNG_COUNT=$(ls -1 *.png 2>/dev/null | wc -l | tr -d ' ')
TOTAL_COUNT=$((ROM_COUNT + PNG_COUNT))

if [ "$TOTAL_COUNT" -eq 0 ]; then
    print_error "No ROM files (.sfc, .smc) or cover art (.png) found in $SOURCE_DIR"
    exit 1
fi

print_info "Found $ROM_COUNT ROM files and $PNG_COUNT cover art files"
echo ""

# Copy each file
COPIED=0
FAILED=0

for file in *.sfc *.smc *.png; do
    # Skip if glob didn't match (file is literal "*.sfc" etc)
    if [ ! -f "$file" ]; then
        continue
    fi

    echo -n "Copying $file... "

    if xcrun devicectl device copy to \
        --device "$DEVICE_ID" \
        --domain-type appDataContainer \
        --domain-identifier "$BUNDLE_ID" \
        --source "$file" \
        --destination "Documents/$file" \
        >/dev/null 2>&1; then

        print_success "✓"
        COPIED=$((COPIED + 1))
    else
        print_error "✗ FAILED"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "======================================"
print_success "Copied: $COPIED files"
if [ "$FAILED" -gt 0 ]; then
    print_error "Failed: $FAILED files"
    exit 1
fi
echo "======================================"
echo ""
print_success "All files copied successfully!"
echo ""
print_info "Next steps:"
echo "1. Launch EZSnes9x on your Apple TV"
echo "2. You should see $ROM_COUNT games in the Cover Flow"
echo "3. If games don't appear, try force-quitting and relaunching the app"
