/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
               This file is licensed under the Snes9x License.
  For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "emulator.h"

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "gfx.h"
#include "snapshot.h"
#include "controls.h"
#include "config.h"
#include "rewind.h"
#include "cpuexec.h"
#include "stream.h"
#include "fscompat.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static S9xConfig s_config;
static std::string s_save_dir;
static std::string s_suspend_path;
static int s_frame_width  = 256;
static int s_frame_height = 224;
static bool s_rewinding = false;

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

// ---------------------------------------------------------------------------
// Emulator namespace implementation
// ---------------------------------------------------------------------------

namespace Emulator {

bool Init(const char *config_path)
{
    memset(&Settings, 0, sizeof(Settings));

    // Sensible defaults
    Settings.Transparency        = true;
    Settings.AutoDisplayMessages = true;
    Settings.SoundPlaybackRate   = 32040; // SNES native audio rate
    Settings.SoundInputRate      = 32040; // Match playback rate
    Settings.Stereo              = true;
    Settings.SixteenBitSound     = true;
    Settings.DynamicRateControl  = true;  // Audio sync via dynamic rate (not SoundSync)
    Settings.DynamicRateLimit    = 5;     // 5% max adjustment
    Settings.SoundSync           = false; // Don't stall emulation for audio
    Settings.FrameTime           = 16667; // ~60 fps
    Settings.StopEmulation       = true;
    Settings.MaxSpriteTilesPerLine = 34; // Enable sprite rendering (34 = standard limit)
    Settings.SuperFXClockMultiplier = 100; // 100% = normal Super FX speed
    Settings.InterpolationMethod = 2; // 0=none, 1=linear, 2=gaussian, 3=cubic, 4=sinc
    Settings.SeparateEchoBuffer  = true;  // Better audio quality for echo effects

    // Default controller setup: pad0 on port 0, pad1 on port 1
    S9xSetController(0, CTL_JOYPAD, 0);
    S9xSetController(1, CTL_JOYPAD, 1);

    // Load config if provided
    if (config_path && config_path[0])
    {
        S9xLoadConfig(config_path, s_config);
        if (!s_config.save_dir.empty())
            s_save_dir = s_config.save_dir;
    }

    if (!Memory.Init())
        return false;

    if (!S9xInitAPU())
    {
        Memory.Deinit();
        return false;
    }

    S9xInitSound(0);

    if (!S9xGraphicsInit())
    {
        S9xDeinitAPU();
        Memory.Deinit();
        return false;
    }

    return true;
}

bool LoadROM(const char *rom_path)
{
    if (!Memory.LoadROM(rom_path))
        return false;

    // Determine save directory
    if (s_save_dir.empty())
    {
        auto path = splitpath(Memory.ROMFilename);
        s_save_dir = path.dir;
    }

    // Set suspend state path
    s_suspend_path = s_save_dir + SLASH_STR + S9xBasenameNoExt(Memory.ROMFilename) + ".suspend";

    // Load SRAM if it exists
    std::string sram_path = S9xGetFilename(".srm", SRAM_DIR);
    Memory.LoadSRAM(sram_path.c_str());

    S9xReset();
    Settings.StopEmulation = false;

    S9xVerifyControllers();

    // Only initialize rewind if enabled in config
    if (s_config.rewind_enabled)
        RewindInit();

    return true;
}

void RunFrame()
{
    S9xMainLoop();

    if (!s_rewinding)
        RewindCapture();
}

void Shutdown()
{
    Settings.StopEmulation = true;

    // Save SRAM
    std::string sram_path = S9xGetFilename(".srm", SRAM_DIR);
    Memory.SaveSRAM(sram_path.c_str());

    RewindDeinit();
    S9xGraphicsDeinit();
    S9xDeinitAPU();
    Memory.Deinit();

    s_save_dir.clear();
    s_suspend_path.clear();
    s_rewinding = false;
}

// Rewind

void RewindStartContinuous()
{
    if (s_rewinding)
        return; // Already rewinding

    s_rewinding = true;
    Settings.Rewinding = true;

    // Immediately enter rewind mode and jump to most recent snapshot
    RewindStep();
}

void RewindStop()
{
    if (!s_rewinding)
        return;

    s_rewinding = false;
    Settings.Rewinding = false;
    RewindRelease();
}

void RewindTick()
{
    if (!s_rewinding)
        return;

    // Step back one snapshot, but don't go past the oldest
    int pos = RewindGetPosition();
    if (pos > 0)
        RewindStep();
    // If pos == 0, we're at the oldest snapshot - freeze here (state already loaded)

    // Run one frame from the restored state to re-render the screen.
    // Rewind snapshots don't include the rendered framebuffer, only PPU/VRAM state,
    // so we need S9xMainLoop to produce visible output.
    // RewindCapture() is skipped because s_rewinding is true.
    S9xMainLoop();
}

bool IsRewinding()
{
    return s_rewinding;
}

int GetRewindBufferDepth()
{
    return RewindGetCount();
}

int GetRewindPosition()
{
    return RewindGetPosition();
}

// Suspend/Resume

void Suspend()
{
    if (Settings.StopEmulation)
        return;

    S9xFreezeGame(s_suspend_path.c_str());

    std::string sram_path = S9xGetFilename(".srm", SRAM_DIR);
    Memory.SaveSRAM(sram_path.c_str());
}

void Resume()
{
    if (Settings.StopEmulation)
        return;

    if (file_exists(s_suspend_path.c_str()))
        S9xUnfreezeGame(s_suspend_path.c_str());
}

// Input

void SetButtonState(int pad, uint16_t buttons)
{
    S9xSetJoypadButtons(pad, (uint16)buttons);
}

// Accessors

const uint16_t *GetFrameBuffer()
{
    return (const uint16_t *)GFX.Screen;
}

int GetFrameWidth()
{
    return s_frame_width;
}

int GetFrameHeight()
{
    return s_frame_height;
}

bool IsPAL()
{
    return Settings.PAL != 0;
}

const char *GetROMName()
{
    return Memory.ROMName;
}

const S9xConfig *GetConfig()
{
    return &s_config;
}

void SetRewindEnabled(bool enabled)
{
    s_config.rewind_enabled = enabled;
}

} // namespace Emulator

// ---------------------------------------------------------------------------
// Frame dimension tracking — called from platform S9xDeinitUpdate/ContinueUpdate
// ---------------------------------------------------------------------------

void EmulatorSetFrameSize(int width, int height)
{
    s_frame_width  = width;
    s_frame_height = height;
}

// ---------------------------------------------------------------------------
// Port interface: platform-specific stubs (identical across all frontends)
// ---------------------------------------------------------------------------

bool8 S9xInitUpdate()
{
    return true;
}

bool8 S9xDeinitUpdate(int width, int height)
{
    EmulatorSetFrameSize(width, height);
    return true;
}

bool8 S9xContinueUpdate(int width, int height)
{
    EmulatorSetFrameSize(width, height);
    return true;
}

void S9xSyncSpeed()
{
    // No-op: timing is handled by the display driver at vsync/swap time.
    //
    // The frontend uses a vsync throttle strategy:
    // 1. Vsync (eglSwapInterval=1 on Android, CVDisplayLink on macOS) provides
    //    the master clock, blocking swap/present at the display refresh rate.
    // 2. FrameThrottle prevents CPU busy-wait by sleeping before swap.
    // 3. DynamicRateControl handles audio drift via +/-5% playback rate adjustment.
    //
    // This approach provides smoother AV sync than the old S9xSyncSpeed sleep loop,
    // which would fight with vsync and cause stutter. With vsync as the clock,
    // small timing differences (60.0Hz display vs 60.1Hz SNES) are absorbed by
    // the audio drift correction rather than causing visible judder.
}

bool8 S9xOpenSoundDevice()
{
    return true;
}

// ---------------------------------------------------------------------------
// Port interface: shared stubs (platform-independent)
// ---------------------------------------------------------------------------

void S9xMessage(int type, int number, const char *message)
{
    (void)type;
    (void)number;
    fprintf(stderr, "%s\n", message);
}

bool8 S9xOpenSnapshotFile(const char *filename, bool8 read_only, STREAM *file)
{
    const char *mode = read_only ? "r" : "w";
    *file = OPEN_STREAM(filename, mode);
    return (*file != nullptr);
}

void S9xCloseSnapshotFile(STREAM file)
{
    CLOSE_STREAM(file);
}

void S9xAutoSaveSRAM()
{
    std::string sram_path = S9xGetFilename(".srm", SRAM_DIR);
    Memory.SaveSRAM(sram_path.c_str());
}

void S9xExit()
{
    Settings.StopEmulation = true;
}

void S9xSetPause(uint32 mask)
{
    Settings.ForcedPause |= mask;
    Settings.Paused = (Settings.ForcedPause != 0);
}

void S9xClearPause(uint32 mask)
{
    Settings.ForcedPause &= ~mask;
    Settings.Paused = (Settings.ForcedPause != 0);
}

// ---------------------------------------------------------------------------
// Port interface: S9xGetDirectory / S9xGetFilenameInc
// ---------------------------------------------------------------------------

std::string S9xGetDirectory(enum s9x_getdirtype type)
{
    (void)type;

    // Use save_dir for all directory types, falling back to ROM directory
    if (!s_save_dir.empty())
        return s_save_dir;

    if (!Memory.ROMFilename.empty())
    {
        auto path = splitpath(Memory.ROMFilename);
        if (!path.dir.empty())
            return path.dir;
    }

    return ".";
}

std::string S9xGetFilenameInc(std::string ext, enum s9x_getdirtype type)
{
    std::string dir = S9xGetDirectory(type);
    std::string stem = S9xBasenameNoExt(Memory.ROMFilename);

    // Find next available number
    for (int i = 0; i < 10000; i++)
    {
        char num[8];
        snprintf(num, sizeof(num), ".%04d", i);
        std::string path = dir + SLASH_STR + stem + num + ext;
        if (!file_exists(path.c_str()))
            return path;
    }

    return dir + SLASH_STR + stem + ext;
}

// ---------------------------------------------------------------------------
