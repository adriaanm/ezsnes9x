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
#include "display.h"
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
    Settings.Transparency        = TRUE;
    Settings.AutoDisplayMessages = TRUE;
    Settings.SoundPlaybackRate   = 48000;
    Settings.Stereo              = TRUE;
    Settings.SixteenBitSound     = TRUE;
    Settings.FrameTime           = 16667; // ~60 fps
    Settings.StopEmulation       = TRUE;

    // Default controller setup: pad0 on port 0, pad1 on port 1
    S9xSetController(0, CTL_JOYPAD, 0, -1, -1, -1);
    S9xSetController(1, CTL_JOYPAD, 1, -1, -1, -1);

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
    Settings.StopEmulation = FALSE;

    S9xVerifyControllers();
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
    Settings.StopEmulation = TRUE;

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

void RewindStart()
{
    s_rewinding = true;
    Settings.Rewinding = TRUE;
}

bool RewindStepBack()
{
    return RewindStep();
}

void RewindEnd()
{
    s_rewinding = false;
    Settings.Rewinding = FALSE;
    RewindRelease();
}

bool IsRewinding()
{
    return s_rewinding;
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

void S9xTextMode()
{
}

void S9xGraphicsMode()
{
}

const char *S9xStringInput(const char *prompt)
{
    (void)prompt;
    return nullptr;
}

void S9xExtraUsage()
{
}

void S9xParseArg(char **argv, int &index, int argc)
{
    (void)argv;
    (void)index;
    (void)argc;
}

void S9xLoadConfigFiles(char **argv, int argc)
{
    (void)argv;
    (void)argc;
}

void S9xUsage()
{
}

void S9xSetTitle(const char *title)
{
    (void)title;
}

void S9xToggleSoundChannel(int c)
{
    static uint8 sound_switch = 255;

    if (c == 8)
        sound_switch = 255;
    else
        sound_switch ^= (1 << c);

    S9xSetSoundControl(sound_switch);
}

const char *S9xSelectFilename(const char *def, const char *dir, const char *ext, const char *title)
{
    (void)def;
    (void)dir;
    (void)ext;
    (void)title;
    return nullptr;
}

void S9xAutoSaveSRAM()
{
    std::string sram_path = S9xGetFilename(".srm", SRAM_DIR);
    Memory.SaveSRAM(sram_path.c_str());
}

void S9xExit()
{
    Settings.StopEmulation = TRUE;
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

void S9xExtraDisplayUsage()
{
}

void S9xParseDisplayArg(char **argv, int &index, int argc)
{
    (void)argv;
    (void)index;
    (void)argc;
}

char *S9xParseArgs(char **argv, int argc)
{
    (void)argv;
    (void)argc;
    return nullptr;
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
// Port interface: polling stubs
// These are called by the controls system when buttons/pointers/axes
// are mapped with poll=TRUE.
// ---------------------------------------------------------------------------

bool S9xPollButton(uint32 id, bool *pressed)
{
    (void)id;
    *pressed = false;
    return false;
}

bool S9xPollPointer(uint32 id, int16 *x, int16 *y)
{
    (void)id;
    *x = 0;
    *y = 0;
    return false;
}

bool S9xPollAxis(uint32 id, int16 *value)
{
    (void)id;
    *value = 0;
    return false;
}

void S9xHandlePortCommand(s9xcommand_t cmd, int16 data1, int16 data2)
{
    (void)cmd;
    (void)data1;
    (void)data2;
}

s9xcommand_t S9xGetPortCommandT(const char *name)
{
    (void)name;
    s9xcommand_t cmd = {};
    return cmd;
}

char *S9xGetPortCommandName(s9xcommand_t command)
{
    (void)command;
    static char buf[] = "";
    return buf;
}

void S9xSetupDefaultKeymap()
{
}

bool8 S9xMapInput(const char *name, s9xcommand_t *cmd)
{
    (void)name;
    (void)cmd;
    return FALSE;
}
