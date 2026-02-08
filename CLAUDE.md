# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Key Documents

- **[PLAN.md](PLAN.md)** — Simplification plan with progress tracking (Phase 1 done, Phase 2 in progress)
- **[LEARNINGS.md](LEARNINGS.md)** — Critical codebase patterns and gotchas (unity builds, Settings struct, port interface)

## Project Overview

Snes9x is a portable SNES emulator being simplified to target only macOS and Android gaming handhelds. The core emulation engine is platform-independent, built as a static library (`libsnes9x-core`).

## Build Commands

```bash
# Configure and build core library
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

There is no test suite. The build itself is the primary verification.

## Architecture

### Core Emulation (root directory)

All core SNES emulation lives in `.cpp`/`.h` files at the repository root. The core has no UI dependencies.

- **CPU (65c816):** `cpuops.cpp` (instruction implementations), `cpuexec.cpp` (execution loop), `cpuaddr.h` (addressing modes), `cpumacro.h` (macros)
- **PPU (graphics):** `ppu.cpp`, `gfx.cpp`, `tile.cpp`, `tileimpl*.cpp`
- **APU (audio):** `apu/` directory — `apu.cpp` (interface), `bapu/dsp/sdsp.cpp` (DSP), `bapu/smp/smp.cpp` (SMP)
- **DMA:** `dma.cpp`
- **Memory/ROM loading:** `memmap.cpp`
- **Save states:** `snapshot.cpp`, `statemanager.cpp`
- **Special cartridge chips:** `sa1.cpp`/`sa1cpu.cpp` (SA-1), `fxemu.cpp`/`fxinst.cpp` (Super FX), `dsp1-4.cpp` (DSP), `sdd1.cpp` (SDD1), `spc7110.cpp` (SPC7110), `c4.cpp` (C4), `obc1.cpp`, `seta*.cpp`
- **Controls:** `controls.cpp` (joypad + multitap only), `snes9x.cpp` (arg parsing)

### Unity Build Pattern (Important!)

Several files `#include` other `.cpp` files and must NOT be compiled directly. See [LEARNINGS.md](LEARNINGS.md) for the full list. Key examples:
- `spc7110.cpp` includes `spc7110emu.cpp` which includes `spc7110dec.cpp`
- `srtc.cpp` includes `srtcemu.cpp`
- `apu/bapu/smp/smp.cpp` includes `algorithms.cpp`, `core.cpp`, etc.

### Port Interface (`display.h`)

Platform frontends must implement functions declared in `display.h`: `S9xPutImage()`, `S9xInitDisplay()`, `S9xProcessEvents()`, `S9xOpenSnapshotFile()`, etc.

### Pixel Format

- macOS: `RGB555` (via `__MACOSX__` define)
- Android/other: `RGB565` (default)

## External Dependencies

- `external/stb/` — stb_image (kept)
- All other externals have been removed

## Removed Features

Debugger, netplay, movie recording, cheats, light gun peripherals (mouse/superscope/justifier/macsrifle), crosshairs, display overlays, compressed ROM loading (zip/jma/gz), old config system, CPU overclock, turbo mode, frame advance, screenshots. All old frontends (gtk/, qt/, win32/, unix/, macosx/, libretro/) and their shared code (common/, filter/) have been deleted.
