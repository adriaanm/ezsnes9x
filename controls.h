/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _CONTROLS_H_
#define _CONTROLS_H_

// Command types for s9xcommand_t
// Only the types used by S9xApplyCommand remain
#define S9xButtonJoypad			1	// Joypad button with turbo/sticky support
#define S9xButtonCommand		2	// Emulator command (Pause, Reset, etc.)
#define S9xAxisJoypad			3	// Analog axis to button mapping

// Port-specific command types (forwarded to S9xHandlePortCommand)
#define S9xButtonPort			251
#define S9xAxisPort				250
#define S9xPointerPort			249

// Simplified command structure for input mapping
// New frontends should use S9xSetJoypadButtons() directly instead
typedef struct
{
	uint8	type;

	union
	{
		// Button commands
		union
		{
			struct
			{
				uint8	idx:3;				// Pad number 0-7
				uint8	toggle:1;			// Toggle turbo/sticky mode
				uint8	turbo:1;			// Turbo button (rapid fire)
				uint8	sticky:1;			// Sticky button (toggle on press)
				uint16	buttons;			// SNES_*_MASK bitmask
			}	joypad;

			uint16	command;				// Emulator command ID
		}	button;

		// Axis commands (analog stick to button mapping)
		struct
		{
			uint8	idx:3;					// Pad number 0-7
			uint8	invert:1;				// Invert axis direction
			uint8	axis:3;				// 0=LR, 1=UD, 2=Y/A, 3=X/B, 4=L/R
			uint8	threshold;				// (threshold+1)/256% = button press threshold
		}	joypad_axis;

		// Port-specific commands
		uint8	port[4];
	};
}	s9xcommand_t;

//////////
// Controller configuration
//////////

enum controllers
{
	CTL_NONE,		// No controller
	CTL_JOYPAD,		// Joypad (use id1 to specify 0-7)
	CTL_MP5,		// Multitap (use id1-id4 to specify pads 0-7 or -1)
};

// Configure which controller is plugged into port 0 or 1
// Called by: platform/shared/emulator.cpp (setup)
void S9xSetController (int port, enum controllers controller, int8 id1, int8 id2, int8 id3, int8 id4); // port=0-1

// Verify controller configuration is valid, returns true if something was disabled
// Called by: platform/shared/emulator.cpp, memmap.cpp
bool S9xVerifyControllers (void);

//////////
// Port interface stubs (implemented by frontend)
//////////

// Port command handling - empty stubs in emulator.cpp
// Called by: controls.cpp internally for legacy S9xApplyCommand support
void S9xHandlePortCommand (s9xcommand_t cmd, int16 data1, int16 data2);
s9xcommand_t S9xGetPortCommandT (const char *name);
char * S9xGetPortCommandName (s9xcommand_t command);
void S9xSetupDefaultKeymap (void);
bool8 S9xMapInput (const char *name, s9xcommand_t *cmd);

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

// Called by: gfx.cpp at end of frame (turbo button processing)
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
	uint8	internal[16];				// joypad[].buttons for 8 pads
};

// Called by: snapshot.cpp
void S9xControlPreSaveState (struct SControlSnapshot *s);
void S9xControlPostLoadState (struct SControlSnapshot *s);

#endif
