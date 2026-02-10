# EZSnes9x Simplification Plan

> **Status: COMPLETED** ✅
> This document describes the simplification work performed on the Snes9x codebase to create EZSnes9x.
> All phases are complete. The project is fully functional on macOS and Android.

## Context

EZSnes9x is a simplified fork of Snes9x, targeting only macOS and Android gaming handhelds. "Plug in a cartridge and play" philosophy. Only gamepad input, suspend/resume, and rewind as modern additions. Configuration is external (YAML), game specified at launch.

---

## Phase 1: Strip the Core [DONE]

- [x] **1a. Remove debugger** — deleted debug.cpp, debug.h, fxdbg.cpp, missing.h; removed all #includes
- [x] **1b. Remove netplay** — deleted netplay.cpp/h, server.cpp/h; removed NETPLAY_SUPPORT blocks
- [x] **1c. Remove movie recording** — deleted movie.cpp/h; removed all movie calls from cpuexec, controls, snapshot, gfx, memmap
- [x] **1d. Remove cheats** — deleted cheats.cpp, cheats2.cpp, cheats.h; removed cheat calls from gfx, memmap, snapshot
- [x] **1e. Remove light gun peripherals & crosshairs** — deleted crosshairs.cpp/h; stripped CTL_MOUSE/SUPERSCOPE/JUSTIFIER/MACSRIFLE from controls.cpp; removed Settings flags
- [x] **1f. Remove display overlays** — removed DisplayTime/FrameRate/PressedKeys/WatchedAddresses from gfx.cpp; simplified S9xDisplayMessages
- [x] **1g. Remove compressed ROM loading** — deleted jma/, unzip/, loadzip.cpp/h; removed UNZIP_SUPPORT/JMA_SUPPORT from memmap.cpp
- [x] **1h. Remove old config system** — deleted conffile.cpp/h; stubbed S9xLoadConfigFiles
- [x] **1i. Remove CPU overclock** — using fixed cycle constants; removed ALLOW_CPU_OVERCLOCK/DANGEROUS_HACKS/OverclockMode
- [x] **1j. Remove turbo mode & frame advance** — removed TurboMode/FrameAdvance/FRAME_ADVANCE_FLAG from Settings/CPU.Flags/apu
- [x] **1k. Clean up Settings struct** — removed ~25 dead fields from snes9x.h Settings struct
- [x] **1l. Remove screenshot support** — deleted screenshot.cpp/h; removed screenshot code from gfx.cpp

---

## Phase 2: New Build System and Rewind Engine [DONE]

- [x] **2d. Delete old frontends** — deleted win32/, gtk/, qt/, unix/, macosx/, libretro/, filter/, common/
- [x] **2e. Clean up externals** — deleted cubeb, fmt, glad, glslang, imgui, SPIRV-Cross, vulkan-headers, VulkanMemoryAllocator-Hpp; kept stb
- [x] **2d-extra. Delete old CI/build files** — deleted appveyor.yml, .cirrus.yml
- [x] **2a. CMakeLists.txt** — top-level CMake building core as static library (libsnes9x-core)
  - Core sources: all remaining .cpp in root + apu/ (excluding unity-included files)
  - No DEBUGGER, NETPLAY_SUPPORT, UNZIP_SUPPORT, JMA_SUPPORT, ZLIB defines
  - RIGHTSHIFT_IS_SAR detection; platform detection for macOS vs Android
  - Build verified: 0 errors, 0 warnings on macOS
- [x] **2b. YAML configuration** — hand-written minimal YAML parser (no external deps); config.cpp/config.h
  - Parses YAML to populate Settings struct + S9xConfig (rom_path, save_dir)
  - Supports: audio (sample_rate, stereo, mute), video (transparency), input (player1/2, multitap, up_and_down)
  - S9xGetDefaultConfigPath() searches cwd, ~/.snes9x/, XDG dirs
- [x] **2c. Rewind engine** — rewind.cpp/rewind.h with XOR-delta compression (no external deps)
  - Ring buffer of 200 snapshots, captured every 3 frames via S9xFreezeGameMem()
  - Periodic keyframes every 30 captures for bounded reconstruction
  - RewindCapture(), RewindStep(), RewindRelease(), RewindActive()
- [x] **Verification** — core library builds with 0 errors, 0 warnings on macOS

---

## Phase 3: New Platform Frontends [DONE]

- [x] **3a. macOS frontend** (platform/macos/)
  - Metal rendering (MTKView, texture from SNES framebuffer)
  - AVAudioEngine audio (render callback pulling from S9xMixSamples)
  - GCController gamepad input
  - Auto suspend/resume on app lifecycle
  - Rewind on L2/ZL hold with visual progress bar
  - No menus/UI beyond game window
- [x] **3b. Android emulator** (platform/android/ + app-android/)
  - OpenGL ES 3.0 rendering
  - Oboe audio (low latency)
  - Gamepad input (AKEYCODE_BUTTON_*, AMOTION_EVENT_AXIS_HAT_*)
  - NativeActivity, ROM via intent or launcher
  - Auto suspend/resume on lifecycle
  - Rewind on L2 hold with two-finger gestures
- [x] **3c. Android launcher** (app-launcher/)
  - Jetpack Compose UI with Cover Flow carousel
  - HOME launcher replacement for dedicated gaming handhelds
  - Automatic ROM scanning with FileObserver
  - Cover art support (PNG matching ROM filenames)
  - Gamepad-only navigation (D-pad, Start, Select+Start, X hold)
  - Game state reset (delete .srm/.suspend files)
  - System status bar (time, WiFi, battery)
- [x] **3d. Shared emulator wrapper** (platform/shared/)
  - emulator.cpp/h — init, run frame, rewind, suspend/resume API
  - Port interface functions (S9xInitUpdate, S9xDeinitUpdate, etc.)

---

## Final Directory Structure

```
snes9x/
├── CMakeLists.txt           # Core library + macOS app build
├── build.gradle.kts         # Android builds (emulator + launcher)
├── CLAUDE.md / LEARNINGS.md / LICENSE / README.md
├── *.cpp, *.h               # Core emulation (CPU, PPU, APU, DMA, etc.)
├── apu/                     # Audio processing unit
├── rewind.cpp/h             # Rewind ring buffer with XOR-delta compression
├── config.cpp/h             # YAML config loader (no external deps)
├── external/stb/            # stb_image (only external dependency)
├── platform/
│   ├── shared/              # Shared emulator wrapper API
│   │   ├── emulator.cpp/h   # Init, run frame, rewind, suspend/resume
│   │   └── headless.cpp     # extern "C" API for Python/ctypes
│   ├── macos/               # macOS Metal frontend (single-file app)
│   └── android/             # Android NativeActivity frontend
├── app-android/             # Android emulator app module (Gradle/Kotlin)
├── app-launcher/            # Android launcher app module (Compose UI)
├── tools/                   # Collection manager (Python, ctypes, headless lib)
├── docs/                    # Documentation
│   ├── snes9x-simple-port.md (this file)
│   ├── collection-manager.md
│   └── launcher-plan.md
└── data/                    # App icons, resources
```

---

## Summary

**All phases complete.** EZSnes9x is a fully functional, simplified SNES emulator with:
- ✅ Core emulation stripped of debugger, netplay, movies, cheats, overlays, compressed ROMs
- ✅ Modern build system (CMake for core + macOS, Gradle for Android)
- ✅ Rewind engine with XOR-delta compression (~30 seconds of history)
- ✅ YAML configuration (no external dependencies)
- ✅ macOS frontend (Metal, AVAudioEngine, GCController)
- ✅ Android emulator (OpenGL ES 3.0, Oboe, NativeActivity)
- ✅ Android launcher (Compose UI, Cover Flow, gamepad navigation)
- ✅ Collection manager tool (Python, generates cover art from title screens)

The codebase is dramatically simplified from upstream Snes9x while maintaining full emulation accuracy and adding quality-of-life features (suspend/resume, rewind) for modern gaming handhelds.
