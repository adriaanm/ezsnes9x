# EZSnes9x Simplification Plan

## Context

EZSnes9x is a simplified fork of Snes9x, targeting only macOS and Android gaming handhelds. "Plug in a cartridge and play" philosophy. Only gamepad input, suspend/resume, and 10-second rewind as modern additions. Configuration is external (YAML), game specified at launch.

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

## Phase 3: New Platform Frontends [PENDING]

- [ ] **3a. macOS frontend** (platform/macos/)
  - Metal rendering (MTKView, texture from SNES framebuffer)
  - AVAudioEngine audio (render callback pulling from S9xMixSamples)
  - GCController gamepad input
  - Auto suspend/resume on app lifecycle
  - Rewind on L2/ZL hold
  - No menus/UI beyond game window
- [ ] **3b. Android frontend** (platform/android/)
  - OpenGL ES 3.0 rendering
  - Oboe audio (low latency)
  - NDK GameController input
  - NativeActivity, ROM via intent
  - Auto suspend/resume on lifecycle
- [ ] **3c. Shared emulator wrapper** (platform/shared/)
  - emulator.cpp/h — init, run frame, rewind, suspend/resume API

---

## Final Directory Structure

```
snes9x/
├── CMakeLists.txt
├── CLAUDE.md / PLAN.md / LICENSE / README.md
├── config.example.yaml
├── *.cpp, *.h              (core emulation)
├── apu/                    (audio processing unit)
├── rewind.cpp/h            (new: rewind ring buffer)
├── config.cpp/h            (new: YAML config loader)
├── external/
│   ├── rapidyaml/          (YAML parsing)
│   ├── lz4/                (rewind compression)
│   └── stb/                (utility)
├── platform/
│   ├── shared/emulator.cpp/h
│   ├── macos/ (main.mm, renderer.mm, audio.mm, input.mm, Info.plist, CMakeLists.txt)
│   └── android/ (NDK app, Gradle build)
└── data/
```
