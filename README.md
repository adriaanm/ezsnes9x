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

### Android

*Coming soon*

## Configuration

Configuration is optional. The emulator uses sensible defaults. If you need to customize settings, create a YAML config file:

```yaml
# Where to store .srm save files (default: same directory as ROM)
save_dir: /path/to/saves

input:
  # Controller assignments (default: pad0 on port 1, pad1 on port 2)
  player1: pad0           # Options: pad0-7, none
  player2: pad1           # Options: pad0-7, none

  # Allow simultaneous opposing directions (default: false)
  up_and_down: false
```

Config file is searched in order:
1. Path specified with `--config`
2. Current working directory (`snes9x.yaml`)
3. `~/.snes9x/config.yaml`
4. XDG config directory

## Controls

### macOS

**Keyboard** (development/testing):
- Arrow keys / WASD: D-pad
- L/K: A/B
- I/O or J: X/Y
- F/P: L/R
- Enter: Start
- Space/Tab: Select

**Game Controller**:
- D-pad: D-pad
- A/B/X/Y: SNES A/B/X/Y
- L1/R1: SNES L/R
- Menu: Start
- Options: Select
- **L2/ZL**: Rewind (hold to rewind, release to resume)

**Mouse**:
- Click: Toggle pause

## Project Status

**Phase 1** (Strip Core): ✅ Complete
- Removed debugger, netplay, movie recording, cheats, light gun peripherals, overlays, compressed ROM loading, old config system, CPU overclock, turbo mode, screenshots
- Cleaned up Settings struct

**Phase 2** (Build System & Rewind): ✅ Complete
- CMake build producing static core library
- YAML configuration parser (no external deps)
- XOR-delta compressed rewind engine (600 snapshots, ~30 seconds)
- Deleted all old frontends and most external dependencies

**Phase 3** (New Frontends): 🚧 In Progress
- macOS frontend: ✅ Complete (Metal, AVAudioEngine, GCController, suspend/resume, rewind with progress bar)
- Android frontend: ⏳ Pending

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
│   └── macos/               # macOS Metal frontend
│       └── main.mm          # Single-file Metal+Audio+Input app
└── data/                    # Resources (icons, etc.)
```

The core emulator builds as a static library (`libsnes9x-core`). Platform frontends link against it and implement the port interface functions declared in `display.h`.

## Documentation

- **[CLAUDE.md](CLAUDE.md)** — Guide for Claude Code when working with this repository
- **[PLAN.md](PLAN.md)** — Detailed simplification plan with progress tracking
- **[LEARNINGS.md](LEARNINGS.md)** — Critical codebase patterns and gotchas
- **[LICENSE](LICENSE)** — Snes9x license

## Credits

Based on [Snes9x](https://github.com/snes9xgit/snes9x) by the Snes9x team.

This fork simplifies the codebase for personal use on macOS and Android gaming handhelds.
