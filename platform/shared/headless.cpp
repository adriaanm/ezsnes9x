/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
               This file is licensed under the Snes9x License.
  For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "emulator.h"

extern "C" {

bool     emu_init(const char *config_path)  { return Emulator::Init(config_path); }
bool     emu_load_rom(const char *path)     { return Emulator::LoadROM(path); }
void     emu_run_frame()                    { Emulator::RunFrame(); }
void     emu_shutdown()                     { Emulator::Shutdown(); }

const uint16_t *emu_framebuffer()           { return Emulator::GetFrameBuffer(); }
int      emu_frame_width()                  { return Emulator::GetFrameWidth(); }
int      emu_frame_height()                 { return Emulator::GetFrameHeight(); }
const char *emu_rom_name()                  { return Emulator::GetROMName(); }
bool     emu_is_pal()                       { return Emulator::IsPAL(); }
void     emu_set_buttons(int pad, uint16_t mask) { Emulator::SetButtonState(pad, mask); }

}
