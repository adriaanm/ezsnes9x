# Android Launcher App

The EZSnes9x launcher is a custom HOME launcher designed for dedicated gaming handhelds. It replaces your Android launcher with a Cover Flow-style interface optimized for gamepad-only navigation.

## Features

- Cover Flow carousel with 3D rotation effects
- Full gamepad navigation (no touch required)
- Cover art support (auto-loads matching PNG files)
- Game state reset (delete saves with confirmation)
- System status bar (time, WiFi, battery)
- System menu access (Settings/Files via button combo)
- Position persistence (remembers last played game)
- Haptic feedback on all interactions
- FileObserver for automatic library updates

## Building

```bash
gradle :app-launcher:assembleRelease
```

**Output:** `app-launcher/build/outputs/apk/release/app-launcher-release.apk`

## Installing

```bash
adb install -r app-launcher/build/outputs/apk/release/app-launcher-release.apk
```

## Setting as Default Launcher

### On-Device

1. Press the **Home** button on your device
2. Select **"EZSnes9x Launcher"**
3. Tap **"Always"** to set as default launcher

### Via ADB

```bash
# Disable stock launcher and set ours as default
adb shell pm disable-user com.android.launcher3
adb shell "cmd package set-home-activity com.ezsnes9x.launcher/.LauncherActivity"
```

## Setting Up Your ROM Library

### Create the ROM Directory

```bash
adb shell mkdir -p /storage/emulated/0/ezsnes9x
```

### Add ROMs

```bash
# Copy individual ROM
adb push GAME_NAME.sfc /storage/emulated/0/ezsnes9x/

# Copy entire directory
adb push /path/to/roms/*.sfc /storage/emulated/0/ezsnes9x/
```

Supported formats: `.sfc`, `.smc`, `.fig`, `.swc`

### Add Cover Art (Optional)

For each ROM, add a matching PNG file with the **same filename**:

```
/storage/emulated/0/ezsnes9x/
├── GAME_NAME_1.sfc
├── GAME_NAME_1.png          ← Cover art
├── GAME_NAME_2.sfc
├── GAME_NAME_2.png          ← Cover art
└── GAME_NAME_3.sfc          ← No cover art (uses colored placeholder)
```

**Naming rules:**
- Cover art **must** have the same base name as the ROM
- Example: `GAME_NAME.sfc` → `GAME_NAME.png`
- Underscores in filenames are replaced with spaces in the UI
- Missing cover art shows a colored placeholder with the game name

**Tip:** Use the [Collection Manager](../tools/collection_manager.py) tool to automatically generate cover art from title screens.

## Controls

| Control | Action |
|---------|--------|
| **D-pad Left/Right** | Navigate through game carousel |
| **Start** | Launch selected game |
| **Select + Start** (hold 1s) | Open system menu (Settings/Files) |
| **X** (hold 1s) | Reset game state (delete saves) |
| **A** | Confirm in dialogs |
| **B** | Cancel in dialogs |

### Touch Controls (Optional)

- Tap center card → Launch game
- Tap side card → Scroll to that card
- Swipe left/right → Navigate carousel
- Pull down → Refresh library (rescan ROMs)

## Resetting Game State

To delete save data for a game:

1. Navigate to the game with **D-pad**
2. **Hold X** for 1 second (haptic feedback when triggered)
3. Confirmation dialog appears
4. Press **A** to confirm → Deletes `.srm` (save data) and `.suspend` (save state)
5. Press **B** to cancel → No changes made

**What gets deleted:**
- `.srm` files: In-game save data (battery-backed SRAM)
- `.suspend` files: Auto-save states created when closing the emulator

**These files are located in the same directory as the ROM.**

## System Menu

Press **Select + Start** (hold 1 second) to access:
- **Android Settings** → Device settings, WiFi, etc.
- **Files** → File manager to add/remove ROMs

This allows system access without leaving the launcher.

## Status Bar

The status bar at the top shows:
- **Left:** WiFi status (green "WiFi" or gray "No WiFi")
- **Center:** Current time (HH:MM:SS, updates every second)
- **Right:** Battery level with color coding:
  - Green: Charging
  - White: >60%
  - Yellow: 30-60%
  - Red: <30%
  - Lightning bolt when plugged in

## File Locations

### ROM Library
```
/storage/emulated/0/ezsnes9x/          # ROM library directory
├── *.sfc, *.smc, *.fig, *.swc         # ROM files
├── *.png                               # Cover art (optional)
├── *.srm                               # Save data (created by emulator)
└── *.suspend                           # Save states (created by emulator)
```

### Preferences
```
/data/data/com.ezsnes9x.launcher/shared_prefs/launcher_prefs.xml
```

## Troubleshooting

### No ROMs Found
- Check that ROMs are in `/storage/emulated/0/ezsnes9x/`
- Tap "Open Files" button to verify path
- Pull down to refresh the library

### Cover Art Not Loading
- Verify PNG filename matches ROM filename exactly
- Check file permissions (should be readable)
- Cover art must be in the same directory as the ROM

### Emulator Not Launching
- Ensure `app-android` (emulator) is installed
- Check logcat: `adb logcat | grep EZSnes9x`
- Reinstall emulator APK

### Launcher Not Appearing in Home Picker
- Verify installation: `adb shell pm list packages | grep ezsnes9x`
- Should show both:
  - `package:com.ezsnes9x.emulator`
  - `package:com.ezsnes9x.launcher`
- Reinstall if missing

### Game State Reset Not Working
- Check file permissions on `/storage/emulated/0/ezsnes9x/`
- `.srm` and `.suspend` files are created by the emulator when you save/exit
- Reset only works if these files exist

### Restoring Default Launcher

To restore the stock Android launcher:
1. Press **Select + Start** (1 second) → "Android Settings"
2. Navigate to **Apps** → **Default apps** → **Home app**
3. Select a different launcher

Or via ADB:
```bash
adb shell pm enable com.android.launcher3
adb shell "cmd package set-home-activity com.android.launcher3/.Launcher"
```

## Technical Details

- **UI Framework:** Jetpack Compose
- **Image Loading:** Coil
- **Permissions:**
  - `MANAGE_EXTERNAL_STORAGE` — Full file access for ROM scanning (Android 11+)
  - `READ_EXTERNAL_STORAGE` — Legacy permission (Android 10 and below)
  - `ACCESS_WIFI_STATE` / `ACCESS_NETWORK_STATE` — Status bar
- **Architecture:** Single-activity Compose UI with StateFlow for reactive updates
- **ROM Scanning:** FileObserver for real-time updates
