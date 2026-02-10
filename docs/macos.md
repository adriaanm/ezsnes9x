# macOS Frontend

The macOS frontend is a native app using Metal for rendering, AVAudioEngine for audio, and GCController for input.

## Features

- Metal-accelerated rendering
- AVAudioEngine audio (low latency)
- GCController support (DualShock, Xbox, Switch, etc.)
- Suspend/resume states
- 30-second rewind with visual progress bar
- ROM specified at launch or via config
- Minimal UI: just the game screen

## Building

```bash
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Output:** `build/platform/macos/ezsnes9x-macos.app`

## Running

### From Terminal

```bash
./build/platform/macos/ezsnes9x-macos.app/Contents/MacOS/ezsnes9x-macos /path/to/rom.sfc
```

### From Finder

Double-click the app bundle, or drag a ROM file onto it.

### ROM Default

If no ROM is specified, the emulator looks for `rom.sfc` in:
1. Current working directory
2. `~/.ezsnes9x/rom.sfc`

## Controls

### Game Controller

| Button | Action |
|--------|--------|
| D-pad | D-pad |
| A/B/X/Y | SNES A/B/X/Y |
| L1/R1 | SNES L/R |
| Menu | Start |
| Options | Select |
| **L2/ZL** | Rewind (hold to rewind, release to resume) |

### Keyboard

Default mapping (configurable in `ezsnes9x.yaml`):

| Key | Action |
|-----|--------|
| Arrow keys | D-pad |
| D | A |
| X | B |
| W | X |
| A | Y |
| Q | L |
| P | R |
| Enter | Start |
| Space | Select |
| **Backspace** | Rewind (hold to rewind, release to resume) |

### Mouse

| Action | Effect |
|--------|--------|
| Click | Pause/unpause (toggle) |

## Configuration

Create `ezsnes9x.yaml` in your home directory (`~/.ezsnes9x/config.yaml`) or project root.

Example:

```yaml
# Rewind (enabled by default)
rewind_enabled: true

# Controller assignment
controller:
  matching: dualshock
  port: 0

# Keyboard configuration
keyboard:
  port: 1
  up: 126
  down: 125
  left: 123
  right: 124
  a: 2
  b: 7
  x: 13
  y: 0
  l: 12
  r: 35
  start: 36
  select: 49
```

See [docs/configuration.md](configuration.md) for full configuration reference.

## Save Data

- **SRAM saves (.srm):** Stored in same directory as ROM
- **Suspend states (.suspend):** Auto-saved on app exit, stored in same directory as ROM

## Technical Details

- **Rendering:** Metal with RGB555 pixel format
- **Audio:** AVAudioEngine with 32-bit float samples
- **Input:** GCController framework
- **File format:** Uncompressed .sfc, .smc, .fig, .swc only
