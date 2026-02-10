# Controls Reference

Complete controller and keyboard mappings for all EZSnes9x platforms.

## macOS Emulator

### Game Controller

| Button | Action |
|--------|--------|
| D-pad | D-pad |
| A/B/X/Y | SNES A/B/X/Y |
| L1/R1 | SNES L/R |
| Menu | Start |
| Options | Select |
| **L2/ZL** | Rewind (hold to rewind, release to resume) |

### Keyboard (Default)

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

---

## Android Emulator (In-Game)

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

---

## Android Launcher App

### Gamepad Controls

| Control | Action |
|---------|--------|
| **D-pad Left/Right** | Navigate through game carousel |
| **Start** | Launch selected game |
| **Select + Start** (hold 1s) | Open system menu (Settings/Files) |
| **X** (hold 1s) | Reset game state (delete saves) |
| **A** | Confirm in dialogs |
| **B** | Cancel in dialogs |

### Touch Controls (Optional)

| Gesture | Action |
|---------|--------|
| Tap center card | Launch game |
| Tap side card | Scroll to that card |
| Swipe left/right | Navigate carousel |
| Pull down | Refresh library (rescan ROMs) |

---

## Rewind Controls

Rewind works the same on all platforms:

| Platform | Control |
|----------|---------|
| macOS (gamepad) | Hold **L2/ZL** |
| macOS (keyboard) | Hold **Backspace** |
| Android (gamepad) | Hold **L2** or two-finger swipe left |

**How it works:**
- Hold the rewind button to rewind time
- Release to resume gameplay
- Rewind buffer is ~30 seconds
- Progress bar shows current position in buffer

---

## Customizing Keyboard Mappings

Keyboard mappings are configurable via `ezsnes9x.yaml` (macOS only). See [docs/configuration.md](configuration.md) for details.

### Finding Keycodes (macOS)

Run with `--debug` and press keys:

```bash
./ezsnes9x-macos --debug /path/to/rom.sfc
# Press keys to see their keycodes
```

---

## Port Assignment

Controllers and keyboards are assigned to SNES controller ports:

1. Controllers with explicit `controller` blocks (matched by name)
2. Remaining controllers auto-assign to free ports in connection order
3. Keyboard assigns to first free port (or specified port)

See [docs/configuration.md](configuration.md) for controller assignment details.
