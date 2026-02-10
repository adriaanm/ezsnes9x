# EZSnes9x

> A stripped-down SNES emulator targeting absolute simplicity with two indulgences: suspend/resume and rewind.

This is a fork of [Snes9x](https://github.com/snes9xgit/snes9x) focused on a "plug in a cartridge and play" philosophy. It supports exactly two platforms:
- **macOS** — Metal rendering, GCController input
- **Android gaming handhelds** — OpenGL ES, gamepad input

See **[BUILD.md](BUILD.md)** for build instructions.

## Philosophy

- **No UI complexity**: No menus, configuration screens, or on-screen displays
- **Gamepad-only**: Keyboard for development/testing, but designed for controllers
- **Two quality-of-life features**:
  - **Suspend/Resume**: Automatic save state on app suspend
  - **Rewind**: 30-second rewind buffer
- **External configuration**: YAML config file, ROM specified at launch
- **Modern codebase**: Removed debugger, netplay, movie recording, cheats, light gun support, display overlays, compressed ROM loading, CPU overclock, turbo mode, screenshots

## Quick Start

**macOS:**
```bash
# Build (see BUILD.md for details)
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# Run
open build/platform/macos/ezsnes9x-macos.app
```

**Android:**
```bash
# Build (see BUILD.md for prerequisites)
gradle assembleRelease
adb install -r app-android/build/outputs/apk/release/app-android-release.apk

# Open any .sfc file from your file manager
```

---

## 📱 Launcher App (Android Gaming Handhelds)

EZSnes9x includes a **custom HOME launcher** designed for dedicated gaming handhelds. It replaces your Android launcher with a Cover Flow-style interface optimized for gamepad-only navigation.

### Features

- 🎨 **Cover Flow carousel** with 3D rotation effects
- 🎮 **Full gamepad navigation** (no touch required)
- 🖼️ **Cover art support** (auto-loads matching PNG files)
- 💾 **Game state reset** (delete saves with confirmation)
- 📊 **System status bar** (time, WiFi, battery)
- 🔒 **System menu access** (Settings/Files via button combo)
- 💫 **Position persistence** (remembers last played game)
- ✨ **Haptic feedback** on all interactions

### Installation

```bash
# Build both apps
gradle assembleRelease

# Install emulator
adb install -r app-android/build/outputs/apk/release/app-android-release.apk

# Install launcher
adb install -r app-launcher/build/outputs/apk/release/app-launcher-release.apk
```

After installation:
1. Press the **Home** button on your device
2. Select **"EZSnes9x Launcher"**
3. Tap **"Always"** to set as default launcher

### Setting Up Your ROM Library

The launcher scans `/storage/emulated/0/ezsnes9x/` for ROMs and cover art.

#### 1. Create the ROM directory

```bash
adb shell mkdir -p /storage/emulated/0/ezsnes9x
```

#### 2. Add ROMs

Copy your `.sfc`, `.smc`, `.fig`, or `.swc` files:

```bash
# Copy individual ROM
adb push GAME_NAME.sfc /storage/emulated/0/ezsnes9x/

# Copy entire directory
adb push /path/to/roms/*.sfc /storage/emulated/0/ezsnes9x/
```

#### 3. Add cover art (optional)

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

**Tip:** Use the [Collection Manager](#collection-manager) tool to automatically generate cover art from title screens.

### Launcher Controls

The launcher is designed for **gamepad-only** use. All controls use the built-in gamepad on Android handhelds (Retroid Pocket, Anbernic, etc.):

| Control | Action |
|---------|--------|
| **D-pad Left/Right** | Navigate through game carousel |
| **Start** | Launch selected game |
| **Select + Start** (hold 1s) | Open system menu (Settings/Files) |
| **X** (hold 1s) | Reset game state (delete saves) |
| **A** | Confirm in dialogs |
| **B** | Cancel in dialogs |

**Touch controls (optional):**
- Tap center card → Launch game
- Tap side card → Scroll to that card
- Swipe left/right → Navigate carousel
- Pull down → Refresh library (rescan ROMs)

### Resetting Game State

To delete save data for a game:

1. Navigate to the game with **D-pad**
2. **Hold X** for 1 second (haptic feedback when triggered)
3. Confirmation dialog appears:
   ```
   Reset Game State?
   Delete save data for: [Game Name]

   This will remove .srm and .suspend files.

   Press A to confirm, B to cancel.
   ```
4. Press **A** to confirm → Deletes `.srm` (save data) and `.suspend` (save state)
5. Press **B** to cancel → No changes made

**What gets deleted:**
- `.srm` files: In-game save data (battery-backed SRAM)
- `.suspend` files: Auto-save states created when closing the emulator

**These files are located in the same directory as the ROM.**

### System Menu Access

Press **Select + Start** (hold 1 second) to access:
- **Android Settings** → Device settings, WiFi, etc.
- **Files** → File manager to add/remove ROMs

This allows system access without leaving the launcher.

### Status Bar

The status bar at the top shows:
- **Left:** WiFi status (green "WiFi" or gray "No WiFi")
- **Center:** Current time (HH:MM:SS, updates every second)
- **Right:** Battery level with color coding:
  - 🟢 Green: Charging
  - ⚪ White: >60%
  - 🟡 Yellow: 30-60%
  - 🔴 Red: <30%
  - ⚡ Lightning bolt when plugged in

### Troubleshooting

**No ROMs found:**
- Check that ROMs are in `/storage/emulated/0/ezsnes9x/`
- Tap "Open Files" button to verify path
- Pull down to refresh the library

**Cover art not loading:**
- Verify PNG filename matches ROM filename exactly
- Check file permissions (should be readable)
- Cover art must be in the same directory as the ROM

**Emulator not launching:**
- Ensure `app-android` (emulator) is installed
- Check logcat: `adb logcat | grep EZSnes9x`
- Reinstall emulator APK

**Launcher not appearing in Home picker:**
- Verify installation: `adb shell pm list packages | grep ezsnes9x`
- Should show both:
  - `package:com.ezsnes9x.emulator`
  - `package:com.ezsnes9x.launcher`
- Reinstall if missing

**Game state reset not working:**
- Check file permissions on `/storage/emulated/0/ezsnes9x/`
- `.srm` and `.suspend` files are created by the emulator when you save/exit
- Reset only works if these files exist

**Resetting the launcher:**
- To restore the default Android launcher:
  - Press **Select + Start** (1 second) → "Android Settings"
  - Navigate to **Apps** → **Default apps** → **Home app**
  - Select a different launcher

### File Locations

All launcher data is stored in:
```
/storage/emulated/0/ezsnes9x/          # ROM library directory
├── *.sfc, *.smc, *.fig, *.swc         # ROM files
├── *.png                               # Cover art (optional)
├── *.srm                               # Save data (created by emulator)
└── *.suspend                           # Save states (created by emulator)
```

Launcher preferences (last selected game):
```
/data/data/com.ezsnes9x.launcher/shared_prefs/launcher_prefs.xml
```

---

## Configuration

Configuration is optional. The emulator uses sensible defaults. If you need to customize settings, create a YAML config file:

```yaml
# Rewind (enabled by default)
rewind_enabled: true         # Set to false to disable rewind feature

# Game controllers auto-assign to ports 0, 1, 2... in connection order
# Override with controller mappings:
controller:
  matching: dualshock    # Substring match (case-insensitive) in controller name
  port: 0                # Assign to port 0 (player 1)

controller:
  matching: xbox
  port: 1                # Assign to port 1 (player 2)

# Keyboard is assigned AFTER controllers (to the first free port)
keyboard:
  port: 1                # Override: assign keyboard to specific port (0-7)
                         # Default: -1 (auto-assign after controllers)

  # Customize key mappings (macOS keycodes shown)
  up: 126                # Arrow up
  down: 125              # Arrow down
  left: 123              # Arrow left
  right: 124             # Arrow right
  a: 2                   # D key
  b: 7                   # X key
  x: 13                  # W key
  y: 0                   # A key
  l: 12                  # Q key
  r: 35                  # P key
  start: 36              # Enter/Return
  select: 49             # Space

# Where to store .srm save files (default: same directory as ROM)
save_dir: /path/to/saves

```

**Finding keycodes:** Run with `--debug` and press keys - unmapped keycodes will be printed to console.

**Controller assignment:** Controllers auto-assign to ports 0, 1, 2... in connection order. Keyboard is assigned to the first free port after all controllers (or override with `keyboard.port`).

Config file is searched in order:
1. Path specified with `--config`
2. Current working directory (`ezsnes9x.yaml`)
3. `~/.ezsnes9x/config.yaml`
4. XDG config directory

## Controls

### macOS (Emulator)

**Keyboard** (configurable, see Configuration section):
- Default mapping: Arrow keys for D-pad, D/X/W/A for A/B/X/Y buttons, Q/P for L/R shoulders, Enter/Space for Start/Select
- **Backspace**: Rewind (hold to rewind time, release to resume)

**Game Controller**:
- D-pad: D-pad
- A/B/X/Y: SNES A/B/X/Y
- L1/R1: SNES L/R
- Menu: Start
- Options: Select
- **L2/ZL**: Rewind (hold to rewind time, release to resume)

**Mouse**:
- Click: Pause/unpause (toggle)

### Android Launcher

See **[Launcher Controls](#launcher-controls)** above for complete gamepad reference.

Quick reference:
- **D-pad Left/Right**: Navigate carousel
- **Start**: Launch game
- **Select + Start** (hold 1s): System menu
- **X** (hold 1s): Reset game state

### Android Emulator (In-Game)

**Built-in Gamepad** (Retroid Pocket, Anbernic, etc.):
- D-pad: D-pad
- A/B/X/Y: SNES A/B/X/Y
- L1/R1: SNES L/R
- Start/Select: Start/Select
- **L2**: Rewind (hold to rewind time, release to resume)
- **Back button**: Exit to launcher

**Touch Gestures**:
- **Two-finger tap**: Pause/unpause (toggle)
- **Two-finger swipe right-to-left**: Rewind (swipe and hold left, or hold L2)

No touch controls or on-screen buttons. Physical gamepad required.

## Project Status

**Phase 1** (Strip Core): ✅ Complete
- Removed debugger, netplay, movie recording, cheats, light gun peripherals, overlays, compressed ROM loading, old config system, CPU overclock, turbo mode, screenshots
- Cleaned up Settings struct

**Phase 2** (Build System & Rewind): ✅ Complete
- CMake build producing static core library
- YAML configuration parser (no external deps)
- XOR-delta compressed rewind engine (600 snapshots, ~30 seconds)
- Deleted all old frontends and most external dependencies

**Phase 3** (New Frontends): ✅ Complete
- macOS frontend: ✅ Complete (Metal, AVAudioEngine, GCController, suspend/resume, rewind with progress bar)
- Android frontend: ✅ Complete (OpenGL ES 3.0, Oboe audio, NativeActivity, gamepad input, suspend/resume, rewind)

## Collection Manager

A Python tool to organize personal ROM collections. It scans a directory of `.sfc` files, extracts game names from ROM headers, runs headless emulation to capture title screen screenshots, and outputs a flat directory of normalized filenames with PNGs alongside each ROM.

```bash
# Build the headless shared library
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# Run (uv auto-installs dependencies)
uv run tools/collection_manager.py /path/to/roms --output-dir /path/to/output --copy-roms
```

Use `--override "GAME_NAME=15"` to set a fixed capture time (in seconds) for games whose title screens don't stabilize automatically. See `docs/collection-manager.md` for details.

## Architecture

```
snes9x/
├── CMakeLists.txt           # Core library build
├── *.cpp, *.h               # Core emulation (CPU, PPU, DMA, cartridge chips)
├── apu/                     # Audio processing unit
├── config.cpp/h             # YAML config parser
├── rewind.cpp/h             # Rewind ring buffer with XOR-delta compression
├── external/stb/            # stb_image (minimal dependency)
├── platform/
│   ├── shared/              # Shared emulator wrapper API
│   │   ├── emulator.cpp/h   # Init, run frame, rewind, suspend/resume
│   │   └── headless.cpp     # extern "C" API for ctypes/FFI
│   ├── macos/               # macOS Metal frontend
│   │   └── main.mm          # Single-file Metal+Audio+Input app
│   └── android/             # Android OpenGL ES frontend
│       ├── main.cpp         # Single-file NativeActivity app
│       └── CMakeLists.txt   # Native library build (links Oboe)
├── app-android/             # Android emulator app (Gradle/Kotlin)
│   └── src/main/            # AndroidManifest, EmulatorActivity.kt
├── app-launcher/            # Android launcher app (Gradle/Kotlin/Compose)
│   └── src/main/            # Cover Flow UI, ROM scanner, gamepad controls
├── tools/
│   └── collection_manager.py  # ROM collection organizer (Python/ctypes)
├── build.gradle.kts         # Root Gradle build (Android)
└── data/                    # Resources (icons, etc.)
```

**Two Android Apps:**
- `app-android` (`com.ezsnes9x.emulator`): The emulator itself (NativeActivity)
- `app-launcher` (`com.ezsnes9x.launcher`): HOME launcher with Cover Flow UI (Compose)

The core emulator builds as a static library (`libsnes9x-core.a`). Platform frontends link against it. The shared emulator wrapper (`emulator.cpp`) provides the high-level API and implements the 5 port interface functions. A headless shared library (`libsnes9x-headless.dylib`) exposes this API via `extern "C"` for scripting with Python/ctypes.

## Documentation

- **[CLAUDE.md](CLAUDE.md)** — Guide for Claude Code when working with this repository
- **[LEARNINGS.md](LEARNINGS.md)** — Critical codebase patterns and gotchas
- **[docs/snes9x-simple-port.md](docs/snes9x-simple-port.md)** — Simplification plan history (completed)
- **[docs/collection-manager.md](docs/collection-manager.md)** — ROM collection organizer tool
- **[docs/launcher-plan.md](docs/launcher-plan.md)** — Android launcher implementation plan
- **[LICENSE](LICENSE)** — Snes9x license

## Credits

Based on [Snes9x](https://github.com/snes9xgit/snes9x) by the Snes9x team.

This fork simplifies the codebase for personal use on macOS and Android gaming handhelds.
