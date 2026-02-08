/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "snes9x.h"
#include <string>
#include <map>

struct S9xKeyboardMapping {
    bool enabled = true;  // Whether keyboard input is enabled
    std::map<std::string, int> button_to_keycode; // e.g., "up" -> 126
};

struct S9xConfig {
    std::string rom_path;
    std::string save_dir;
    S9xKeyboardMapping keyboard;
};

// Parse config file and populate Settings struct + S9xConfig
bool S9xLoadConfig(const char *filename, S9xConfig &config);

// Get default config path (looks for snes9x.yml in standard locations)
std::string S9xGetDefaultConfigPath();

#endif
