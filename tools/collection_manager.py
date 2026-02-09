#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["Pillow"]
# ///
"""Scan SNES ROMs, capture title screenshots, generate metadata."""

import argparse
import ctypes
import hashlib
import json
import shutil
import sys
from datetime import datetime
from pathlib import Path

from PIL import Image

MAX_SNES_WIDTH = 512
SNES_START_MASK = 1 << 12


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
    lib.emu_set_buttons.argtypes = [ctypes.c_int, ctypes.c_uint16]
    lib.emu_set_buttons.restype = None

    return lib


def framebuffer_hash(lib) -> bytes:
    """Hash the visible portion of the framebuffer."""
    w, h = lib.emu_frame_width(), lib.emu_frame_height()
    buf = lib.emu_framebuffer()
    md5 = hashlib.md5()
    for y in range(h):
        row = (ctypes.c_uint16 * w).from_address(
            ctypes.addressof(buf.contents) + y * MAX_SNES_WIDTH * 2
        )
        md5.update(bytes(row))
    return md5.digest()


def screen_complexity(lib) -> int:
    """Count unique pixel values in the framebuffer. More = visually richer."""
    w, h = lib.emu_frame_width(), lib.emu_frame_height()
    buf = lib.emu_framebuffer()
    colors = set()
    for y in range(h):
        for x in range(w):
            colors.add(buf[y * MAX_SNES_WIDTH + x])
    return len(colors)


def is_blank(lib) -> bool:
    """Check if the framebuffer is nearly all one color."""
    return screen_complexity(lib) < 4


def capture_screenshot(lib) -> Image.Image:
    w, h = lib.emu_frame_width(), lib.emu_frame_height()
    buf = lib.emu_framebuffer()

    pixels = bytearray(w * h * 3)
    for y in range(h):
        for x in range(w):
            p = buf[y * MAX_SNES_WIDTH + x]
            i = (y * w + x) * 3
            pixels[i] = ((p >> 10) & 0x1F) << 3
            pixels[i + 1] = ((p >> 5) & 0x1F) << 3
            pixels[i + 2] = (p & 0x1F) << 3

    return Image.frombytes("RGB", (w, h), bytes(pixels))


def run_and_collect_stable_screens(lib, max_frames, stable_needed):
    """Run emulation, collecting screenshots each time the screen stabilizes.

    Returns list of (image, complexity, frame_number) for each stable screen.
    """
    candidates = []
    prev_hash = None
    stable = 0
    total = 0

    while total < max_frames:
        lib.emu_run_frame()
        total += 1
        h = framebuffer_hash(lib)
        if h == prev_hash:
            stable += 1
            if stable == stable_needed:
                complexity = screen_complexity(lib)
                if not is_blank(lib):
                    candidates.append((capture_screenshot(lib), complexity, total))
        else:
            stable = 0
            prev_hash = h

    return candidates, total


def press_start(lib):
    """Press and release Start button."""
    lib.emu_set_buttons(0, SNES_START_MASK)
    for _ in range(10):
        lib.emu_run_frame()
    lib.emu_set_buttons(0, 0)


def capture_title_screen(lib):
    """Capture the best title screen screenshot.

    Strategy:
    1. Run up to 1800 frames (~30s), collecting every stable screen
    2. Pick the most visually complex one (title screens have more colors
       than publisher logos or copyright text)
    3. If best candidate looks like a logo (< 60 colors) or nothing found,
       press Start to reach a menu and try again
    """
    # Phase 1: skip initial boot
    for _ in range(60):
        lib.emu_run_frame()

    # Phase 2: collect stable screens over ~28 seconds
    candidates, frames = run_and_collect_stable_screens(
        lib, max_frames=1740, stable_needed=45
    )

    all_candidates = list(candidates)
    best = max(all_candidates, key=lambda c: c[1]) if all_candidates else None

    # If we have a rich stable screen (>= 60 colors), use it
    if best and best[1] >= 60:
        return best[0], f"best of {len(all_candidates)} stable screens at {60 + best[2]}f ({best[1]} colors)"

    # Phase 3: press Start to skip past intros/logos to a menu
    press_start(lib)
    candidates2, frames2 = run_and_collect_stable_screens(
        lib, max_frames=900, stable_needed=45
    )
    all_candidates.extend(candidates2)

    best = max(all_candidates, key=lambda c: c[1]) if all_candidates else None
    if best and best[1] >= 60:
        return best[0], f"after Start, best of {len(all_candidates)} ({best[1]} colors)"

    # Phase 4: press Start again (some games need two presses)
    press_start(lib)
    candidates3, _ = run_and_collect_stable_screens(
        lib, max_frames=600, stable_needed=45
    )
    all_candidates.extend(candidates3)

    best = max(all_candidates, key=lambda c: c[1]) if all_candidates else None
    if best:
        return best[0], f"best of {len(all_candidates)} after 2x Start ({best[1]} colors)"

    # Phase 5: last resort — take whatever is on screen
    for _ in range(300):
        lib.emu_run_frame()

    if is_blank(lib):
        return capture_screenshot(lib), "blank"
    return capture_screenshot(lib), "best effort"


def sanitize(name: str) -> str:
    """Convert ROM header name to a clean filename with underscores."""
    out = []
    for c in name.strip():
        if c.isalnum():
            out.append(c)
        elif out and out[-1] != "_":
            out.append("_")
    return "".join(out).strip("_")


def capture_fixed(lib, seconds: float):
    """Simple fixed-time capture: run N seconds, take screenshot."""
    frames = int(seconds * 60)
    for _ in range(frames):
        lib.emu_run_frame()
    return capture_screenshot(lib), f"fixed {seconds}s"


def process_rom(lib, rom_path: Path, output_dir: Path, copy_rom: bool,
                overrides: dict[str, float] | None = None) -> dict | None:
    if not lib.emu_load_rom(str(rom_path).encode()):
        print(f"  SKIP: failed to load {rom_path.name}", file=sys.stderr)
        return None

    name = lib.emu_rom_name().decode("ascii", errors="replace").strip()
    is_pal = lib.emu_is_pal()
    safe_name = sanitize(name)

    # Check for a timing override (match against sanitized name, case-insensitive)
    override_secs = None
    if overrides:
        for key, secs in overrides.items():
            if sanitize(key).lower() == safe_name.lower():
                override_secs = secs
                break

    if override_secs is not None:
        img, strategy = capture_fixed(lib, override_secs)
    else:
        img, strategy = capture_title_screen(lib)

    img.save(output_dir / f"{safe_name}.png")
    print(f"  {rom_path.name} -> {safe_name}  ({strategy})")

    if copy_rom:
        shutil.copy2(rom_path, output_dir / f"{safe_name}.sfc")

    return {
        "original_path": str(rom_path),
        "game_name": name,
        "filename": safe_name,
        "region": "PAL" if is_pal else "NTSC",
        "screenshot_strategy": strategy,
    }


def main():
    parser = argparse.ArgumentParser(description="SNES ROM collection manager")
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("./output"))
    parser.add_argument("--lib", type=Path, default=Path("./build/libsnes9x-headless.dylib"))
    parser.add_argument("--copy-roms", action="store_true", help="copy ROMs to output dir with normalized names")
    parser.add_argument("--override", action="append", metavar="NAME=SECONDS",
                        help="fixed capture timing for specific games (e.g. 'GAME_NAME=15')")
    args = parser.parse_args()

    # Parse overrides: NAME=SECONDS
    overrides = {}
    for o in args.override or []:
        if "=" not in o:
            parser.error(f"--override must be NAME=SECONDS, got: {o}")
        name, secs = o.rsplit("=", 1)
        overrides[name] = float(secs)

    lib = load_library(args.lib)
    if not lib.emu_init(b""):
        sys.exit("Failed to init emulator")

    args.output_dir.mkdir(parents=True, exist_ok=True)

    roms = sorted(args.rom_directory.rglob("*.sfc"))
    print(f"Found {len(roms)} ROMs")
    if overrides:
        print(f"Overrides: {overrides}")

    results = []
    for rom in roms:
        info = process_rom(lib, rom, args.output_dir, args.copy_roms, overrides)
        if info:
            results.append(info)

    lib.emu_shutdown()

    metadata = {"generated": datetime.now().isoformat(), "roms": results}
    meta_path = args.output_dir / "metadata.json"
    meta_path.write_text(json.dumps(metadata, indent=2))
    print(f"\nWrote {meta_path} ({len(results)} ROMs)")


if __name__ == "__main__":
    main()
