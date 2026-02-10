# Configuration

Configuration is optional. The emulator uses sensible defaults. If you need to customize settings, create a YAML config file.

## Config File Locations

The config file is searched in order:
1. Path specified with `--config` command-line flag
2. Current working directory: `./ezsnes9x.yaml`
3. Home directory: `~/.ezsnes9x/config.yaml`
4. XDG config directory: `$XDG_CONFIG_HOME/ezsnes9x/config.yaml`

## Full Config Example

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

## Options Reference

### rewind_enabled

Enable or disable the rewind feature. When disabled, you cannot rewind gameplay.

- **Type:** Boolean
- **Default:** `true`
- **Platforms:** macOS, Android

### controller

Assign a specific controller to a specific port. Controllers are matched by substring (case-insensitive) against their device name.

- **matching:** Substring to match in controller name
- **port:** Player port (0-7)

**Example:**
```yaml
controller:
  matching: dualshock
  port: 0
```

Multiple controller blocks are supported. The first matching controller for each port wins.

### keyboard

Configure keyboard port assignment and key mappings.

- **port:** Player port (0-7), or -1 for auto-assign
- **up, down, left, right, a, b, x, y, l, r, start, select:** Platform-specific keycodes

#### Finding Keycodes (macOS)

Run the emulator with `--debug` and press keys. Unmapped keycodes will be printed to console:

```bash
./ezsnes9x-macos --debug /path/to/rom.sfc
# Press keys to see their keycodes
```

#### macOS Default Keycodes

| Action | Key | Code |
|--------|-----|------|
| Up | Arrow Up | 126 |
| Down | Arrow Down | 125 |
| Left | Arrow Left | 123 |
| Right | Arrow Right | 124 |
| A | D | 2 |
| B | X | 7 |
| X | W | 13 |
| Y | A | 0 |
| L | Q | 12 |
| R | P | 35 |
| Start | Enter | 36 |
| Select | Space | 49 |

### save_dir

Directory to store `.srm` save files. If not specified, saves are stored in the same directory as the ROM.

- **Type:** Path (string)
- **Default:** Same directory as ROM
- **Platforms:** macOS, Android

## Controller Assignment Order

1. Controllers with explicit `controller` blocks (matched by name)
2. Remaining controllers auto-assign to free ports in connection order
3. Keyboard assigns to first free port (or specified port)

**Example:** With 2 DualShock controllers connected:
```yaml
controller:
  matching: dualshock
  port: 0  # First DualShock → Player 1

# Second DualShock → Player 2 (auto-assigned)
# Keyboard → Player 3 (auto-assigned)
```

## Platform-Specific Notes

### macOS

- Keycodes are macOS virtual keycodes (CGKeyCode)
- Controller names use GCController's vendor/product strings
- Config file in `~/.ezsnes9x/config.yaml`

### Android

- Keyboard configuration not applicable (no keyboard input in-game)
- Controller assignment done via gamepad auto-detection
- `save_dir` not supported (saves always stored with ROM)

## Disabling Rewind

To disable rewind for better performance or reduced memory usage:

```yaml
rewind_enabled: false
```

This frees the rewind buffer and disables all rewind functionality.
