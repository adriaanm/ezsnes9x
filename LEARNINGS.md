# Snes9x Codebase Learnings

## Unity Build Pattern (Critical)

Many snes9x source files use a "unity build" pattern where `.cpp` files `#include` other `.cpp` files. These included files must NOT be compiled directly or you'll get duplicate symbol / missing type errors.

**Key unity-included files (do NOT compile directly):**
- `spc7110emu.cpp` and `spc7110dec.cpp` — both included by `spc7110.cpp`
- `srtcemu.cpp` — included by `srtc.cpp`
- `apu/bapu/smp/algorithms.cpp`, `core.cpp`, `iplrom.cpp`, `memory.cpp`, `timing.cpp` — all included by `smp.cpp`
- `apu/bapu/smp/core/oppseudo_*.cpp` — included by `core.cpp`
- `apu/bapu/dsp/SPC_DSP.cpp` — included by `sdsp.cpp`
- `apu/bapu/smp/debugger/*.cpp` — included by `smp.cpp` (under `#ifdef DEBUGGER`)

**Files that DO need direct compilation despite including other .cpp files:**
- `sa1cpu.cpp` — re-includes `cpuops.cpp` with `#define` remapping for SA-1 coprocessor. Both `sa1cpu.cpp` and `cpuops.cpp` must be compiled.

**How to identify:** Look for `#ifdef _FILENAME_CPP_` guards at the top of files — these are meant to be included, not compiled directly. Also grep for `#include "*.cpp"` patterns.

## Settings Struct References Spread Widely

When removing `Settings` struct fields (like `DisplayColor`, `TurboMode`, `TakeScreenshot`), references exist not just in the obvious files but also in:
- `memmap.cpp` — ROM loading uses `DisplayColor` for status messages, `TakeScreenshot` reset
- `controls.cpp` — command handler switch cases reference almost every Settings field
- `apu/apu.cpp` — references `TurboMode` for sound sync

Always do a full codebase grep for any removed field before building.

## #ifdef Feature Guards

Many features are cleanly `#ifdef`-guarded and can be removed just by not defining the macro:
- `DEBUGGER` — guards debug code in ~33 files including APU debugger
- `NETPLAY_SUPPORT` — guards netplay in a few files
- `UNZIP_SUPPORT` / `JMA_SUPPORT` — compressed ROM loading
- `ALLOW_CPU_OVERCLOCK` — CPU cycle constants
- `ZLIB` — gzip stream support in snes9x.h

## Controller System Architecture

`controls.cpp` is the most complex file in the codebase (~3700 lines). It has:
- An internal command enum (`enum command_numbers`) with hundreds of entries
- A mapping system from abstract commands to controller types
- Save state serialization that must maintain byte-offset compatibility (use `SKIP` macros when removing struct fields)
- The command handler `S9xReportButton()` switch statement that references nearly every Settings field

## Platform Port Interface

`display.h` defines the interface that platform frontends must implement:
- `S9xPutImage()` — display frame
- `S9xInitDisplay()` / `S9xDeinitDisplay()` — setup/teardown
- `S9xToggleSoundChannel()` — audio control
- `S9xOpenSnapshotFile()` / `S9xCloseSnapshotFile()` — save state I/O
- `S9xProcessEvents()` — input polling
- `S9xExtraUsage()` / `S9xParseArg()` — CLI extensions
- `S9xMessage()` — declared in `snes9x.h`

## Pixel Format

- macOS uses `RGB555` (set via `__MACOSX__` define in `port.h`)
- Everything else defaults to `RGB565`
- `BUILD_PIXEL` macro from `pixform.h` creates pixels in the active format

## APU Structure

The APU lives in `apu/` with a sub-structure:
- `apu/apu.cpp` — main APU interface (compile directly)
- `apu/bapu/smp/smp.cpp` — SMP processor (compile directly, unity-includes sub-files)
- `apu/bapu/smp/smp_state.cpp` — SMP state save/load (compile directly)
- `apu/bapu/dsp/sdsp.cpp` — DSP processor (compile directly, includes SPC_DSP.cpp)
- `apu/resampler.h` — audio resampling (header-only)

## Rewind System

The rewind system (`rewind.cpp`) uses a circular buffer with XOR delta compression:

**Buffer structure:**
- Ring buffer of 600 snapshot slots (configurable via `RING_CAPACITY`)
- Default captures 1 snapshot every 3 frames = ~30 seconds of history at 60fps
- Each slot stores either:
  - **Keyframe**: Full emulator state (~100KB per state)
  - **Delta**: XOR difference from previous state (compression by storing only changes)
- Keyframes inserted every 30 captures to bound reconstruction time

**Memory usage:**
- When enabled: Allocates ring buffer + 2 scratch buffers on `RewindInit()`
- When disabled: No allocation (buffer size = 0) to save memory
- Total memory: ~600 slots * average size (varies based on keyframe ratio)

**How it works:**
1. **Capture** (`RewindCapture()`): Called every frame, stores state every 3 frames
2. **Rewind** (`RewindStep()`): Walk backwards in ring, reconstruct state from nearest keyframe + deltas
3. **Release** (`RewindRelease()`): Discard snapshots newer than cursor position

**Important notes:**
- Rewind is initialized in `Emulator::LoadROM()` based on `s_config.rewind_enabled`
- Frontend can override config via `Emulator::SetRewindEnabled()` before loading ROM
- Reconstruction walks back to nearest keyframe, then replays XOR deltas forward
- The `s_prev_state` buffer tracks previous keyframe for delta generation

## Build System Notes

- CMake 3.20+ works well with the codebase
- On macOS, `Unix Makefiles` generator works (Ninja may not be installed)
- The codebase compiles cleanly with `-Wall` plus a few `-Wno-*` flags
- `HAVE_STDINT_H` and `HAVE_STRINGS_H` should be defined for modern systems
- No external dependencies needed for the core library (all self-contained)
