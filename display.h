/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef SNES9X_DISPLAY_H_
#define SNES9X_DISPLAY_H_

#include "snes9x.h"

// Port interface functions that must be implemented by the frontend

void S9xMessage(int type, int number, const char *message);
bool8 S9xOpenSnapshotFile(const char *filename, bool8 read_only, STREAM *file);
void S9xCloseSnapshotFile(STREAM file);

#endif
