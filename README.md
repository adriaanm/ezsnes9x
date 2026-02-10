# EZSnes9x

> A stripped-down SNES emulator targeting absolute simplicity with two indulgences: **suspend/resume** and **rewind**.

This is a fork of [Snes9x](https://github.com/snes9xgit/snes9x) with the sole aim of casual vintage gaming, as close to the good old "plug in a cartridge and play" as possible. Two modern conveniences: your game will be ready to resume where you left it, and, when you missed that jump again, just rewind to travel back in time.

**Platforms:** macOS and Android gaming handhelds only.

**Build instructions:** See [BUILDING.md](BUILDING.md)

## Disclaimer

This is a personal project implemented using AI. No binaries are provided.

Although I'm using and enjoying this code, I make no claims regarding its quality or utility, nor do I make any commitments regarding fixing bugs or accepting changes. All AI-generated code in this repo is licensed under the Unlicense.

## Credits

Based on [Snes9x](https://github.com/snes9xgit/snes9x) by the Snes9x team.

---

## Features

- **No UI complexity:** No menus, configuration screens, or on-screen displays
- **Gamepad-only:** Designed for controllers (keyboard supported on Mac)
- **30-second rewind:** Hold a button to rewind time, release to resume
- **Auto-suspend:** Save state created automatically on app exit

## Controls
- Rewind: hold L2 on controller, backspace on keyboard, or two-finger swipe left (android).
- To pause while the app is open: two-finger tap (android) or click (mac).

See [docs/controls.md](docs/controls.md) for complete control reference.

## What's Included

### Frontends

| Platform | Rendering | Audio | Input | Docs |
|----------|-----------|-------|-------|------|
| **macOS** | Metal | AVAudioEngine | GCController / keyboard | [docs/macos.md](docs/macos.md) |
| **Android (emulator)** | OpenGL ES 3.0 | Oboe | Native gamepad | [docs/android.md](docs/android.md) |
| **Android (launcher)** | Jetpack Compose | — | Gamepad & touch | [docs/launcher.md](docs/launcher.md) |

## Android Launcher

EZSnes9x includes a custom HOME launcher for Android gaming handhelds. Cover Flow carousel, gamepad & touch navigation, cover art support, system status bar (time/WiFi/battery), game state reset.

See [docs/launcher.md](docs/launcher.md) for setup and usage.

### Tools

- **Collection Manager** — Organize ROM collections, generate cover art from title screens ([docs/collection-manager.md](docs/collection-manager.md))

## Quick Start

**macOS:**
```bash
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
open build/platform/macos/ezsnes9x-macos.app
```

**Android:**
```bash
gradle assembleRelease
adb install -r app-android/build/outputs/apk/release/app-android-release.apk
# Open any .sfc file from your file manager
```

For full build instructions, see [BUILDING.md](BUILDING.md).

## Documentation

- **[BUILDING.md](BUILDING.md)** — Build instructions for all platforms
- **[docs/macos.md](docs/macos.md)** — macOS frontend
- **[docs/android.md](docs/android.md)** — Android emulator
- **[docs/launcher.md](docs/launcher.md)** — Android launcher app
- **[docs/controls.md](docs/controls.md)** — Complete control reference
- **[docs/configuration.md](docs/configuration.md)** — YAML config reference
- **[docs/architecture.md](docs/architecture.md)** — Codebase architecture
- **[docs/collection-manager.md](docs/collection-manager.md)** — ROM collection tool
- **[docs/snes9x-simple-port.md](docs/snes9x-simple-port.md)** simplification history
