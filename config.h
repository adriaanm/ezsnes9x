/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "snes9x.h"
#include <string>

struct S9xConfig {
    std::string rom_path;
    std::string save_dir;
};

// Parse config file and populate Settings struct + S9xConfig
bool S9xLoadConfig(const char *filename, S9xConfig &config);

// Get default config path (looks for snes9x.yml in standard locations)
std::string S9xGetDefaultConfigPath();

#endif
