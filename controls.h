/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _CONTROLS_H_
#define _CONTROLS_H_

//////////
// Controller configuration
//////////

enum controllers
{
	CTL_NONE,		// No controller
	CTL_JOYPAD,		// Joypad (use id1 to specify 0-7)
};

// Configure which controller is plugged into port 0 or 1
// Called by: platform/shared/emulator.cpp (setup)
void S9xSetController (int port, enum controllers controller, int8 id1);

// Verify controller configuration is valid, returns true if something was disabled
// Called by: platform/shared/emulator.cpp, memmap.cpp
bool S9xVerifyControllers (void);

//////////
// Direct joypad interface (preferred for new frontends)
//////////

// Set joypad button state directly - this is the main input function for new frontends
// Called by: platform/shared/emulator.cpp
// pad: 0-7, buttons: SNES_*_MASK bitmask from snes9x.h
void S9xSetJoypadButtons (int pad, uint16 buttons);

//////////
// Core emulator callbacks (called by PPU/CPU/etc)
//////////

// Called by: ppu.cpp on reset
void S9xControlsReset (void);
void S9xControlsSoftReset (void);

// Called by: ppu.cpp when writing to $4016 (joypad latch)
void S9xSetJoypadLatch (bool latch);

// Called by: ppu.cpp when reading $4016/$4017 (joypad serial)
uint8 S9xReadJOYSERn (int n);

// Called by: gfx.cpp at end of frame
void S9xControlEOF (void);

//////////
// Save state support
//////////

struct SControlSnapshot
{
	uint8	ver;
	uint8	port1_read_idx[2];
	uint8	dummy1[4];					// for future expansion
	uint8	port2_read_idx[2];
	uint8	dummy2[4];
	uint8	dummy3[8];
	bool8	pad_read, pad_read_last;
	uint8	internal[16];				// joypad[].buttons for 8 joypads
};

// Called by: snapshot.cpp
void S9xControlPreSaveState (struct SControlSnapshot *s);
void S9xControlPostLoadState (struct SControlSnapshot *s);

#endif
