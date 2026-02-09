# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Key Documents

- **[PLAN.md](PLAN.md)** — Simplification plan with progress tracking (Phase 1 done, Phase 2 in progress)
- **[LEARNINGS.md](LEARNINGS.md)** — Critical codebase patterns and gotchas (unity builds, Settings struct, port interface)

## Project Overview

Snes9x is a portable SNES emulator being simplified to target only macOS and Android gaming handhelds. The core emulation engine is platform-independent, built as a static library (`libsnes9x-core`).

## Build Commands

### Host Build (Core Library)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Android Build (APK)

```bash
# Set up local.properties with SDK/NDK paths
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties  # macOS
echo "ndk.dir=$HOME/Library/Android/sdk/ndk/27.1.12297006" >> local.properties

# Build debug APK (Gradle at repo root invokes CMake via AGP)
gradle assembleDebug
```

**Output:** `app-android/build/outputs/apk/debug/app-android-debug.apk` (~6MB)

**Key points:**
- Gradle build is at repo root (moved from platform/android/ for simpler CMake path resolution)
- AGP invokes CMake automatically via `externalNativeBuild` in app-android/build.gradle.kts
- `app-android/build.gradle.kts` references `../../CMakeLists.txt` (repo root)
- No manual CMake steps needed for APK builds

### Android Cross-Compile (Standalone .so)

For building just the native library without APK (e.g., for ARM64 Linux hosts):

```bash
# Clone Oboe manually (FetchContent fails with some toolchain setups)
git clone --depth 1 --branch 1.9.0 https://github.com/google/oboe.git /tmp/oboe

# Configure with platform toolchain
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=platform/android/toolchain-macos.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_OBOE=/tmp/oboe

# Build
cmake --build build-android -j$(nproc)
```

**Note:** There is no test suite. The build itself is the primary verification.

## Architecture

### Platform Structure

- **`/` (root):** Core emulation engine — platform-independent, built as static library `libsnes9x-core.a`
- **`platform/shared/`:** Shared emulator wrapper (`emulator.cpp`) — high-level API used by both frontends
- **`platform/macos/`:** macOS frontend — Metal + AVAudioEngine + GCController, builds app bundle
- **`platform/android/`:** Android frontend — OpenGL ES 3.0 + Oboe + NativeActivity, builds native lib
- **`app-android/`:** Android app module (Gradle/Kotlin) — at repo root for simpler CMake paths

### Core Emulation (root directory)

All core SNES emulation lives in `.cpp`/`.h` files at the repository root. The core has no UI dependencies.

- **CPU (65c816):** `cpuops.cpp` (instruction implementations), `cpuexec.cpp` (execution loop), `cpuaddr.h` (addressing modes), `cpumacro.h` (macros)
- **PPU (graphics):** `ppu.cpp`, `gfx.cpp`, `tile.cpp`, `tileimpl*.cpp`
- **APU (audio):** `apu/` directory — `apu.cpp` (interface), `bapu/dsp/sdsp.cpp` (DSP), `bapu/smp/smp.cpp` (SMP)
- **DMA:** `dma.cpp`
- **Memory/ROM loading:** `memmap.cpp`
- **Save states:** `snapshot.cpp`, `statemanager.cpp`
- **Special cartridge chips:** `sa1.cpp`/`sa1cpu.cpp` (SA-1), `fxemu.cpp`/`fxinst.cpp` (Super FX), `dsp1-4.cpp` (DSP), `sdd1.cpp` (SDD1), `spc7110.cpp` (SPC7110), `c4.cpp` (C4), `obc1.cpp`, `seta*.cpp`
- **Controls:** `controls.cpp` (joypad + multitap only)

### Unity Build Pattern (Important!)

Several files `#include` other `.cpp` files and must NOT be compiled directly. See [LEARNINGS.md](LEARNINGS.md) for the full list. Key examples:
- `spc7110.cpp` includes `spc7110emu.cpp` which includes `spc7110dec.cpp`
- `srtc.cpp` includes `srtcemu.cpp`
- `apu/bapu/smp/smp.cpp` includes `algorithms.cpp`, `core.cpp`, etc.

### Port Interface

Frontends must implement 5 platform-specific port functions:

- **`S9xInitUpdate()`** — Called before frame rendering starts (return true)
- **`S9xDeinitUpdate(int w, int h)`** — Called after frame rendering completes; `w`/`h` are final dimensions
- **`S9xContinueUpdate(int w, int h)`** — Called for interlaced fields
- **`S9xSyncSpeed()`** — Frame rate throttling (no-op if vsync handles it)
- **`S9xOpenSoundDevice()`** — Audio init (return true)

All other port functions (file I/O, input polling, etc.) are implemented in `platform/shared/emulator.cpp`.

### Pixel Format

- macOS: `RGB555` (via `__MACOSX__` define)
- Android/other: `RGB565` (default)

**Important:** The framebuffer pitch is `MAX_SNES_WIDTH` pixels regardless of actual frame width. When uploading to GPU textures, set `GL_UNPACK_ROW_LENGTH` to `MAX_SNES_WIDTH` before calling `glTexSubImage2D()`.

### Android Frontend Details

**Architecture:** Single-file C++ NativeActivity (`platform/android/main.cpp`) with thin Kotlin shim for intent handling.

**Entry points:**
- `ANativeActivity_onCreate` — Exported from `native_app_glue` static library (linked with `--whole-archive`)
- `android_main()` — Main loop, called by app glue

**Dependencies:**
- Oboe (audio) — Fetched via CMake `FetchContent` from GitHub
- Android NDK — native_app_glue, EGL, GLESv3, android, log

**NDK API usage:**
- `ALooper_pollOnce()` — Event loop (not `ALooper_pollAll`, which is deprecated in r27+)
- `AInputEvent` — Gamepad input (`AKEYCODE_BUTTON_*`, `AMOTION_EVENT_AXIS_HAT_*`)
- JNI — ROM path extraction from `Intent.getDataString()` (file:// URIs from file manager)

**Intent handling:**
- File managers send VIEW intents with `file://` URIs in `intent.getData()`
- Must call `intent.getDataString()` then strip `file://` prefix and URL-decode
- URL decoding handles spaces (`%20`) and special characters in paths
- Fallback path: `/storage/emulated/0/Android/data/com.snes9x.emulator/files/rom.sfc`

**Build quirks:**
- Gradle at repo root — simpler CMake path resolution (`../../CMakeLists.txt`)
- AGP 8.7.3 + Kotlin 2.1.0 — modern versions for Android 14 support
- `requestLegacyExternalStorage="true"` — for Android 10 scoped storage compatibility
- `READ_EXTERNAL_STORAGE` permission — required for accessing ROM files

## External Dependencies

- `external/stb/` — stb_image (kept)
- All other externals have been removed

## Removed Features

Debugger, netplay, movie recording, cheats, light gun peripherals (mouse/superscope/justifier/macsrifle), crosshairs, display overlays, compressed ROM loading (zip/jma/gz), old config system, CPU overclock, turbo mode, frame advance, screenshots. All old frontends (gtk/, qt/, win32/, unix/, macosx/, libretro/) and their shared code (common/, filter/) have been deleted.
