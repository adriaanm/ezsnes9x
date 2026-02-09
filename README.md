# Snes9x — Simplified Fork

> A stripped-down SNES emulator targeting absolute simplicity with two indulgences: suspend/resume and rewind.

This is a fork of [Snes9x](https://github.com/snes9xgit/snes9x) focused on a "plug in a cartridge and play" philosophy. It supports exactly two platforms:
- **macOS** — Metal rendering, GCController input
- **Android gaming handhelds** — OpenGL ES, gamepad input

## Philosophy

- **No UI complexity**: No menus, configuration screens, or on-screen displays
- **Gamepad-only**: Keyboard for development/testing, but designed for controllers
- **Two quality-of-life features**:
  - **Suspend/Resume**: Automatic save state on app suspend
  - **Rewind**: 30-second rewind buffer (hold L2/ZL trigger)
- **External configuration**: YAML config file, ROM specified at launch
- **Modern codebase**: Removed debugger, netplay, movie recording, cheats, light gun support, display overlays, compressed ROM loading, CPU overclock, turbo mode, screenshots

## Building

### macOS

```bash
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
./build/platform/macos/snes9x-macos.app/Contents/MacOS/snes9x-macos path/to/rom.sfc
```

Optional flags:
- `--config path/to/config.yaml` — Load configuration file
- `--debug` — Enable debug logging
- `--no-rewind` — Disable rewind feature

### Android

#### Prerequisites

- Android SDK with NDK r27+ (Android Studio, or SDK with NDK via `sdkmanager`)
- Java 17+ (for Gradle)
- Gradle (or use Gradle Wrapper)

#### Build APK

```bash
# Create local.properties with SDK/NDK paths
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties  # macOS
echo "sdk.dir=$HOME/Android/Sdk" > local.properties         # Linux

# Build debug APK
gradle assembleDebug
# Or: ./gradlew assembleDebug  (if using Gradle Wrapper)
```

**Output:** `app-android/build/outputs/apk/debug/app-android-debug.apk` (~6MB)

#### Installation

```bash
# Install APK
adb install app-android/build/outputs/apk/debug/app-android-debug.apk

# Launch via file manager: open any .sfc/.smc/.fig/.swc file
# Or launch directly:
adb shell am start -n com.snes9x.emulator/.EmulatorActivity
```

The app supports opening ROMs directly from any file manager (uses VIEW intent with file:// URIs).

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
2. Current working directory (`snes9x.yaml`)
3. `~/.snes9x/config.yaml`
4. XDG config directory

## Controls

### macOS

**Keyboard** (configurable, see Configuration section):
- Default mapping: Arrow keys for D-pad, A/W/X/D for Y/X/B/A buttons, Q/P for L/R shoulders, Enter/Space for Start/Select
- **Backspace**: Rewind (hold to rewind, release to resume)

**Game Controller**:
- D-pad: D-pad
- A/B/X/Y: SNES A/B/X/Y
- L1/R1: SNES L/R
- Menu: Start
- Options: Select
- **L2/ZL**: Rewind (hold to rewind, release to resume)

**Mouse**:
- Click: Toggle pause

### Android

**Built-in Gamepad** (Retroid Pocket, Anbernic, etc.):
- D-pad: D-pad
- A/B/X/Y: SNES A/B/X/Y
- L1/R1: SNES L/R
- Start/Select: Start/Select
- **L2**: Rewind (hold to rewind, release to resume)

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
├── app-android/             # Android app module (Gradle/Kotlin)
│   └── src/main/            # AndroidManifest, EmulatorActivity.kt
├── tools/
│   └── collection_manager.py  # ROM collection organizer (Python/ctypes)
├── build.gradle.kts         # Root Gradle build (Android)
└── data/                    # Resources (icons, etc.)
```

The core emulator builds as a static library (`libsnes9x-core.a`). Platform frontends link against it. The shared emulator wrapper (`emulator.cpp`) provides the high-level API and implements the 5 port interface functions. A headless shared library (`libsnes9x-headless.dylib`) exposes this API via `extern "C"` for scripting with Python/ctypes.

## Documentation

- **[CLAUDE.md](CLAUDE.md)** — Guide for Claude Code when working with this repository
- **[PLAN.md](PLAN.md)** — Detailed simplification plan with progress tracking
- **[LEARNINGS.md](LEARNINGS.md)** — Critical codebase patterns and gotchas
- **[LICENSE](LICENSE)** — Snes9x license

## Credits

Based on [Snes9x](https://github.com/snes9xgit/snes9x) by the Snes9x team.

This fork simplifies the codebase for personal use on macOS and Android gaming handhelds.
