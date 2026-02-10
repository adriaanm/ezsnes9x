# Architecture

Technical overview of the EZSnes9x codebase structure and design.

## Directory Structure

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

## Component Overview

### Core Emulation Engine

Platform-independent SNES emulation built as a static library (`libsnes9x-core.a`).

**Components:**
- **CPU (65c816):** `cpuops.cpp`, `cpuexec.cpp`, `cpuaddr.h`, `cpumacro.h`
- **PPU (graphics):** `ppu.cpp`, `gfx.cpp`, `tile.cpp`, `tileimpl*.cpp`
- **APU (audio):** `apu/` directory — `apu.cpp`, `bapu/dsp/sdsp.cpp`, `bapu/smp/smp.cpp`
- **DMA:** `dma.cpp`
- **Memory/ROM loading:** `memmap.cpp`
- **Save states:** `snapshot.cpp`, `statemanager.cpp`
- **Special cartridge chips:** `sa1.cpp`, `fxemu.cpp`, `dsp1-4.cpp`, `sdd1.cpp`, `spc7110.cpp`, `c4.cpp`, `obc1.cpp`, `seta*.cpp`
- **Controls:** `controls.cpp` (joypad + multitap only)
- **Rewind:** `rewind.cpp` (XOR-delta compressed ring buffer)
- **Configuration:** `config.cpp` (YAML parser)

### Shared Emulator Wrapper

High-level API used by all frontends. Located in `platform/shared/emulator.cpp`.

**Key functions:**
- `Emulator::init()` — Initialize core and port interface
- `Emulator::loadRom()` — Load ROM file
- `Emulator::runFrame()` — Execute one frame
- `Emulator::rewind()` — Rewind N frames
- `Emulator::saveState()` / `loadState()` — Suspend/resume

**Port Interface:** Frontends implement 5 platform-specific functions:
- `S9xInitUpdate()` — Called before frame rendering
- `S9xDeinitUpdate(int w, int h)` — Called after frame rendering
- `S9xContinueUpdate(int w, int h)` — Called for interlaced fields
- `S9xSyncSpeed()` — Frame rate throttling
- `S9xOpenSoundDevice()` — Audio init

All other port functions (file I/O, input polling) are implemented in `platform/shared/emulator.cpp`.

### Platform Frontends

#### macOS (`platform/macos/main.mm`)

Single-file Objective-C++ app with:
- **Rendering:** Metal (`MTKView` delegate)
- **Audio:** AVAudioEngine (32-bit float, low latency)
- **Input:** GCController framework
- **Entry:** `NSApplicationMain` with app delegate

#### Android (`platform/android/main.cpp`)

Single-file C++ NativeActivity with:
- **Rendering:** OpenGL ES 3.0
- **Audio:** Oboe (16-bit signed)
- **Input:** Native app glue with `AInputEvent` API
- **Entry:** `ANativeActivity_onCreate` → `android_main()`

Thin Kotlin shim (`EmulatorActivity.kt`) extracts ROM path from file:// URIs.

#### Android Launcher (`app-launcher/`)

Jetpack Compose UI with:
- **Cover Flow carousel:** 3D rotation effects
- **ROM scanning:** FileObserver for real-time updates
- **Gamepad controls:** `onPreviewKeyEvent` for button handling
- **No native code:** Pure Kotlin/Compose

### Tools

#### Collection Manager (`tools/collection_manager.py`)

Python tool using `ctypes` to call headless shared library:
- Scans ROM directory
- Extracts game names from ROM headers
- Runs headless emulation to capture title screens
- Outputs normalized filenames with PNG cover art

## Two Android Apps

- **`app-android`** (`com.ezsnes9x.emulator`): The emulator itself (NativeActivity)
- **`app-launcher`** (`com.ezsnes9x.launcher`): HOME launcher with Cover Flow UI (Compose)

They communicate via intents — the launcher sends VIEW intents with file:// URIs to the emulator.

## Pixel Formats

- **macOS:** RGB555 (via `__MACOSX__` define)
- **Android/other:** RGB565 (default)

**Important:** The framebuffer pitch is `MAX_SNES_WIDTH` pixels regardless of actual frame width. When uploading to GPU textures, set `GL_UNPACK_ROW_LENGTH` to `MAX_SNES_WIDTH` before calling `glTexSubImage2D()`.

## Unity Build Pattern

Several files `#include` other `.cpp` files and must NOT be compiled directly. See [LEARNINGS.md](../LEARNINGS.md) for the full list.

Key examples:
- `spc7110.cpp` includes `spc7110emu.cpp` which includes `spc7110dec.cpp`
- `srtc.cpp` includes `srtcemu.cpp`
- `apu/bapu/smp/smp.cpp` includes `algorithms.cpp`, `core.cpp`, etc.

## Build System

### CMake (Core + Native Libraries)

- Core library: Static lib `libsnes9x-core.a`
- Headless library: Shared lib `libsnes9x-headless.dylib` (for Python/ctypes)
- Android native lib: Linked via Gradle AGP

### Gradle (Android Apps)

- Root `build.gradle.kts` configures both apps
- AGP invokes CMake via `externalNativeBuild`
- `app-android/build.gradle.kts` references `../../CMakeLists.txt`

## External Dependencies

- `external/stb/` — stb_image (kept)
- Oboe (audio) — Fetched via CMake `FetchContent` from GitHub
- All other externals have been removed

## Removed Features

Debugger, netplay, movie recording, cheats, light gun peripherals (mouse/superscope/justifier/macsrifle), crosshairs, display overlays, compressed ROM loading (zip/jma/gz), old config system, CPU overclock, turbo mode, screenshots. All old frontends (gtk/, qt/, win32/, unix/, macosx/, libretro/) and their shared code (common/, filter/) have been deleted.
