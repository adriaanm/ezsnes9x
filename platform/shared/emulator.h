/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
               This file is licensed under the Snes9x License.
  For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include <cstdint>

struct S9xConfig;

namespace Emulator {
    bool Init(const char *config_path);         // Load config, init memory/APU/graphics
    bool LoadROM(const char *rom_path);         // Load ROM, set up controllers, reset
    void RunFrame();                             // S9xMainLoop() + rewind capture
    void Shutdown();                             // Save SRAM, deinit everything
    const S9xConfig *GetConfig();                // Access loaded config (e.g., keyboard mapping)

    // Rewind
    void RewindStartContinuous();          // Start continuous rewind (call on trigger down)
    void RewindStop();                     // Stop rewinding and resume forward play (call on trigger release)
    void RewindTick();                     // Step back one snapshot if rewinding (call every frame)
    bool IsRewinding();
    int GetRewindBufferDepth();            // Total snapshots available
    int GetRewindPosition();               // Current position (0 = oldest, depth-1 = newest)

    // Suspend/Resume (app lifecycle)
    void Suspend();                        // Save state to temp file + save SRAM
    void Resume();                         // Restore state from temp file

    // Input (frontend calls these)
    void SetButtonState(int pad, uint16_t buttons);  // Set joypad bitmask directly

    // Accessors
    const uint16_t *GetFrameBuffer();      // -> GFX.Screen
    int GetFrameWidth();
    int GetFrameHeight();
    bool IsPAL();
    const char *GetROMName();
}

#endif
