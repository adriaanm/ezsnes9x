# ROM Collection Manager

## Overview

A Python tool that scans SNES ROM files, extracts game names from ROM headers, runs headless emulation to capture title screen screenshots, and generates metadata JSON for use in a ROM launcher.

## Architecture

### Layered design

```
tools/collection_manager.py      Python script (scanning, screenshots, metadata)
        │ ctypes
        ▼
platform/shared/headless.cpp     extern "C" API over Emulator namespace (~30 lines)
        │
        ▼
platform/shared/emulator.cpp     Existing shared wrapper (init, run, accessors)
        │
        ▼
libsnes9x-core                   Core emulation engine (static lib)
```

### Key insight: emulator.cpp already does almost everything

The existing `Emulator::` namespace in `platform/shared/emulator.cpp` provides:

- `Init()` / `LoadROM()` / `RunFrame()` / `Shutdown()` — full lifecycle
- `GetFrameBuffer()` / `GetFrameWidth()` / `GetFrameHeight()` — framebuffer access
- `GetROMName()` / `IsPAL()` — ROM metadata
- `SetButtonState()` — input (unused for headless, but available)
- Port functions: `S9xMessage`, `S9xOpenSnapshotFile`, `S9xCloseSnapshotFile`, `S9xExit`, etc.

What's missing are the **5 platform-specific port stubs** (`S9xInitUpdate`, `S9xDeinitUpdate`, `S9xContinueUpdate`, `S9xSyncSpeed`, `S9xOpenSoundDevice`). Currently these are duplicated identically in both `platform/macos/main.mm` and `platform/android/main.cpp`. They belong in shared code.

### Changes to shared code (benefits all frontends)

**Move the 5 port stubs into `emulator.cpp`:**

The implementations are identical across both platforms — return TRUE and call `EmulatorSetFrameSize()`. Moving them into `emulator.cpp` means:
- macOS and Android frontends get ~25 lines shorter
- Any future frontend (headless, testing, etc.) gets them for free
- Zero behavior change — the stubs are pure no-ops

**Add `platform/shared/headless.cpp`** (~30 lines):

A thin `extern "C"` wrapper exposing the `Emulator::` namespace to ctypes. This is the **only new C++ file** needed. It exists because:
- Python ctypes cannot call C++ namespaced functions or methods
- `extern "C"` prevents name mangling, giving stable symbol names
- The functions are trivial one-line delegations

```cpp
// platform/shared/headless.cpp
#include "emulator.h"
extern "C" {
    bool     emu_init(const char *cfg)          { return Emulator::Init(cfg); }
    bool     emu_load_rom(const char *path)     { return Emulator::LoadROM(path); }
    void     emu_run_frame()                    { Emulator::RunFrame(); }
    void     emu_shutdown()                     { Emulator::Shutdown(); }
    const uint16_t *emu_framebuffer()           { return Emulator::GetFrameBuffer(); }
    int      emu_frame_width()                  { return Emulator::GetFrameWidth(); }
    int      emu_frame_height()                 { return Emulator::GetFrameHeight(); }
    const char *emu_rom_name()                  { return Emulator::GetROMName(); }
    bool     emu_is_pal()                       { return Emulator::IsPAL(); }
}
```

### Why each piece of C++ is unavoidable

| Code | Lines | Why it can't be Python |
|------|-------|----------------------|
| 5 port stubs in `emulator.cpp` | ~20 | Core calls these during emulation. Without implementations, the shared library won't link. They're the contract between core and frontend. |
| `headless.cpp` extern "C" wrappers | ~15 | ctypes needs C linkage symbols. C++ name mangling produces compiler-specific symbols like `_ZN8Emulator4InitEPKc` that would break across compilers/versions. |

**Total new C++:** ~15 lines (headless.cpp). The port stubs are moved, not new.

### Why Python + ctypes?

- **Rapid iteration:** Experiment with screenshot timing without recompiling
- **Rich ecosystem:** Pillow for images, built-in JSON/pathlib
- **Zero setup:** `uv run` with inline script metadata — no virtualenv, no `pip install`

## Implementation

### Phase 1: Shared library build

- [ ] Move 5 port stubs from `platform/macos/main.mm` and `platform/android/main.cpp` into `platform/shared/emulator.cpp`
- [ ] Create `platform/shared/headless.cpp` with `extern "C"` wrappers
- [ ] Add CMake target `snes9x-headless` (SHARED library):
  ```cmake
  add_library(snes9x-headless SHARED
      platform/shared/emulator.cpp
      platform/shared/headless.cpp
  )
  target_link_libraries(snes9x-headless PRIVATE snes9x-core)
  ```
- [ ] Verify macOS and Android frontends still build (they no longer define the 5 stubs)
- [ ] Verify `libsnes9x-headless.dylib` loads in Python: `ctypes.CDLL("./build/libsnes9x-headless.dylib")`

### Phase 2: Python tool

- [ ] Create `tools/collection_manager.py` with inline script metadata for `uv run`
- [ ] Implement: scan ROMs, load each, run 300 frames, capture screenshot, save PNG + JSON

The complete Python tool is ~150 lines:

```python
#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["Pillow"]
# ///
"""Scan SNES ROMs, capture title screenshots, generate metadata."""

import argparse
import ctypes
import json
import struct
import sys
from datetime import datetime
from pathlib import Path
from PIL import Image

MAX_SNES_WIDTH = 512

def load_library(lib_path: Path):
    lib = ctypes.CDLL(str(lib_path))

    lib.emu_init.argtypes = [ctypes.c_char_p]
    lib.emu_init.restype = ctypes.c_bool
    lib.emu_load_rom.argtypes = [ctypes.c_char_p]
    lib.emu_load_rom.restype = ctypes.c_bool
    lib.emu_run_frame.restype = None
    lib.emu_shutdown.restype = None
    lib.emu_framebuffer.restype = ctypes.POINTER(ctypes.c_uint16)
    lib.emu_frame_width.restype = ctypes.c_int
    lib.emu_frame_height.restype = ctypes.c_int
    lib.emu_rom_name.restype = ctypes.c_char_p
    lib.emu_is_pal.restype = ctypes.c_bool

    return lib

def capture_screenshot(lib) -> Image.Image:
    w, h = lib.emu_frame_width(), lib.emu_frame_height()
    buf = lib.emu_framebuffer()

    # Read framebuffer (pitch = MAX_SNES_WIDTH, not w)
    pixels = bytearray(w * h * 3)
    for y in range(h):
        for x in range(w):
            p = buf[y * MAX_SNES_WIDTH + x]
            i = (y * w + x) * 3
            # RGB555: RRRR RGGG GGBB BBB0  (bit 0 unused)
            pixels[i]     = ((p >> 10) & 0x1F) << 3
            pixels[i + 1] = ((p >>  5) & 0x1F) << 3
            pixels[i + 2] = ( p        & 0x1F) << 3

    return Image.frombytes("RGB", (w, h), bytes(pixels))

def process_rom(lib, rom_path: Path, output_dir: Path, frames: int) -> dict:
    if not lib.emu_load_rom(str(rom_path).encode()):
        print(f"  SKIP: failed to load {rom_path.name}", file=sys.stderr)
        return None

    name = lib.emu_rom_name().decode("ascii", errors="replace").strip()
    is_pal = lib.emu_is_pal()
    print(f"  {rom_path.name} → {name}")

    for _ in range(frames):
        lib.emu_run_frame()

    img = capture_screenshot(lib)
    screenshot_name = f"{sanitize(name)}.png"
    img.save(output_dir / "screenshots" / screenshot_name)

    return {
        "original_filename": rom_path.name,
        "game_name": name,
        "screenshot": screenshot_name,
        "region": "PAL" if is_pal else "NTSC",
    }

def sanitize(name: str) -> str:
    return "".join(c if c.isalnum() or c in " -_()" else "_" for c in name).strip()

def main():
    parser = argparse.ArgumentParser(description="SNES ROM collection manager")
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("./output"))
    parser.add_argument("--frames", type=int, default=300, help="frames to run before capture")
    parser.add_argument("--lib", type=Path, default=Path("./build/libsnes9x-headless.dylib"))
    args = parser.parse_args()

    lib = load_library(args.lib)
    if not lib.emu_init(b""):
        sys.exit("Failed to init emulator")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "screenshots").mkdir(exist_ok=True)

    roms = sorted(args.rom_directory.glob("*.sfc"))
    print(f"Found {len(roms)} ROMs")

    results = []
    for rom in roms:
        info = process_rom(lib, rom, args.output_dir, args.frames)
        if info:
            results.append(info)

    lib.emu_shutdown()

    metadata = {"generated": datetime.now().isoformat(), "roms": results}
    meta_path = args.output_dir / "metadata.json"
    meta_path.write_text(json.dumps(metadata, indent=2))
    print(f"Wrote {meta_path} ({len(results)} ROMs)")

if __name__ == "__main__":
    main()
```

### Phase 3: Screenshot timing (iterate)

Start with fixed 300 frames. If title screens aren't great, try:

- **Scene stability:** Hash framebuffer each frame, capture after 60 frames of no change
- **Maximum frames with timeout:** Run up to 600 frames, capture the one with most unique colors (likely title screen, not black/loading)

These are pure Python changes — no recompilation needed.

## Usage

```bash
# Build shared library
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run (uv auto-installs Pillow)
uv run tools/collection_manager.py /path/to/roms

# Custom settings
uv run tools/collection_manager.py /path/to/roms --output-dir ./collection --frames 600
```

### Output

```
output/
├── metadata.json
└── screenshots/
    ├── GAME_TITLE_1.png
    ├── GAME_TITLE_2.png
    └── ...
```

## Dependencies

- **Python:** Pillow (auto-managed by `uv run` via inline script metadata)
- **System:** CMake 3.20+, C++17 compiler
- **No numpy** — Pillow handles all image I/O; pixel conversion is done with plain Python

## Notes

### Pixel format

macOS builds use RGB555 (`__MACOSX__` define). The framebuffer pitch is always `MAX_SNES_WIDTH` (512) regardless of actual frame width.

### Thread safety

The core uses global state and is not thread-safe. Process ROMs serially. The tool calls `emu_load_rom` → run frames → capture → next ROM in a single process.

### ROM lifecycle

Each `emu_load_rom()` call resets the core and loads a new ROM. SRAM is loaded/saved automatically by `emulator.cpp`. For headless screenshot capture, SRAM state doesn't matter — every game starts fresh.

## Progress

- [ ] Phase 1: Shared library build (move port stubs, add headless.cpp, CMake target)
- [ ] Phase 2: Python tool (collection_manager.py)
- [ ] Phase 3: Screenshot timing experiments
