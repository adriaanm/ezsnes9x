# Android Emulator Frontend

The Android emulator is a native app using OpenGL ES 3.0 for rendering, Oboe for audio, and NativeActivity with gamepad input.

## Features

- OpenGL ES 3.0 rendering
- Oboe low-latency audio
- NativeActivity (no Java UI)
- Gamepad-only design (no touch controls)
- Suspend/resume states
- 30-second rewind
- Opens ROMs from file manager intents

## Building

### Setup

Configure `local.properties`:

```bash
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties
echo "ndk.dir=$HOME/Library/Android/sdk/ndk/27.1.12297006" >> local.properties
```

### Build

```bash
gradle :app-android:assembleRelease
```

**Output:** `app-android/build/outputs/apk/release/app-android-release.apk` (~6MB)

## Installing

```bash
adb install -r app-android/build/outputs/apk/release/app-android-release.apk
```

## Running

### From File Manager

1. Open any file manager app
2. Navigate to your ROM file
3. Tap the ROM file
4. Select "EZSnes9x" when prompted

### From Launcher (Cover Flow)

See [docs/launcher.md](launcher.md) for the dedicated launcher app.

### Command Line (ADB)

```bash
adb shell am start -n com.snes9x.emulator/.EmulatorActivity -a android.intent.action.VIEW -d "file:///storage/emulated/0/rom.sfc"
```

## Controls

### Built-in Gamepad

| Button | Action |
|--------|--------|
| D-pad | D-pad |
| A/B/X/Y | SNES A/B/X/Y |
| L1/R1 | SNES L/R |
| Start | Start |
| Select | Select |
| **L2** | Rewind (hold to rewind, release to resume) |
| **Back** | Exit to launcher |

### Touch Gestures

| Gesture | Action |
|---------|--------|
| Two-finger tap | Pause/unpause (toggle) |
| Two-finger swipe right-to-left | Rewind (swipe and hold left, or hold L2) |

**Note:** No on-screen buttons. Physical gamepad required.

## ROM Locations

The emulator can load ROMs from any location via file manager intents. For the launcher, ROMs are stored in:

```
/storage/emulated/0/ezsnes9x/
```

## Save Data

- **SRAM saves (.srm):** Stored in same directory as ROM
- **Suspend states (.suspend):** Auto-saved on app exit, stored in same directory as ROM
- **Location:** `/storage/emulated/0/ezsnes9x/` or ROM's source directory

## Permissions

- `MANAGE_EXTERNAL_STORAGE` — Full file access for ROM loading (Android 11+)
- `REQUEST_INSTALL_PACKAGES` — For APK installation (optional)

## Technical Details

- **Rendering:** OpenGL ES 3.0 with RGB565 pixel format
- **Audio:** Oboe with 16-bit signed samples
- **Input:** Native app glue with AInputEvent API
- **Entry point:** `ANativeActivity_onCreate` (via native_app_glue)
- **Intent handling:** Kotlin shim extracts ROM path from file:// URIs

## Troubleshooting

### ROM Not Loading

- Verify `MANAGE_EXTERNAL_STORAGE` permission is granted
- Check logcat: `adb logcat | grep EZSnes9x`
- Ensure ROM file path is accessible

### Audio Issues

- Check Oboe initialization in logcat
- Verify device supports low-latency audio

### Controller Not Detected

- Check logcat for controller detection: `adb logcat | grep -i input`
- Some devices require button mapping configuration

### App Crashes on Launch

- Check logcat for crash details
- Verify NDK version (r27+ required)
- Clean rebuild: `gradle clean && gradle assembleRelease`
