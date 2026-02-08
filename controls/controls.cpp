/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "snapshot.h"
#include "controls.h"
#include "display.h"

#define NONE					(-2)
#define JOYPAD0					0
#define JOYPAD1					1
#define JOYPAD2					2
#define JOYPAD3					3
#define JOYPAD4					4
#define JOYPAD5					5
#define JOYPAD6					6
#define JOYPAD7					7
#define NUMCTLS					8

#define FLAG_IOBIT0				(Memory.FillRAM[0x4213] & 0x40)
#define FLAG_IOBIT1				(Memory.FillRAM[0x4213] & 0x80)
#define FLAG_IOBIT(n)			((n) ? (FLAG_IOBIT1) : (FLAG_IOBIT0))

bool8	pad_read = 0, pad_read_last = 0;
uint8	read_idx[2 /* ports */][2 /* per port */];

static struct
{
	uint16	buttons;
}	joypad[8];

static bool8		FLAG_LATCH = FALSE;
static int32		curcontrollers[2] = { NONE, NONE };
static int32		newcontrollers[2] = { JOYPAD0, NONE };

// Full reset — just delegates to soft reset since there's no additional hardware state to clear.
void S9xControlsReset (void)
{
	S9xControlsSoftReset();
}

// Soft reset — zeroes the serial read indices, clears the latch, and applies
// any pending controller configuration (newcontrollers → curcontrollers).
void S9xControlsSoftReset (void)
{
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
			read_idx[i][j] = 0;

	FLAG_LATCH = FALSE;

	curcontrollers[0] = newcontrollers[0];
	curcontrollers[1] = newcontrollers[1];
}

// Configure which controller type is plugged into a SNES port (0 or 1).
// The change is staged in newcontrollers[] and only takes effect on the next latch
// transition (1→0) or reset.  id1 selects which joypad slot (0-7) to bind.
void S9xSetController (int port, enum controllers controller, int8 id1)
{
	if (port < 0 || port > 1)
		return;

	switch (controller)
	{
		case CTL_NONE:
			newcontrollers[port] = NONE;
			break;

		case CTL_JOYPAD:
			if (id1 >= 0 && id1 <= 7)
				newcontrollers[port] = JOYPAD0 + id1;
			else
				newcontrollers[port] = NONE;
			break;

		default:
			fprintf(stderr, "Unknown controller type %d\n", controller);
			newcontrollers[port] = NONE;
			break;
	}
}

// Validate the pending controller configuration in newcontrollers[].
// Disables duplicate joypad assignments (same pad on both ports).
// Returns true if any configuration was changed.
bool S9xVerifyControllers (void)
{
	bool	ret = false;
	int		port, i, used[NUMCTLS];

	for (i = 0; i < NUMCTLS; used[i++] = 0) ;

	for (port = 0; port < 2; port++)
	{
		switch (i = newcontrollers[port])
		{
			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				if (used[i - JOYPAD0]++ > 0)
				{
					char	buf[256];
					snprintf(buf, sizeof(buf), "Joypad%d used more than once! Disabling extra instances", i - JOYPAD0 + 1);
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, buf);
					newcontrollers[port] = NONE;
					ret = true;
				}
				break;

			default:
				break;
		}
	}

	return (ret);
}

// Handle writes to $4016 (joypad latch register).
// On rising edge (0→1): resets the serial read indices so the next reads
// start from bit 0.  On falling edge (1→0): applies any pending controller
// configuration from newcontrollers[] into curcontrollers[].
void S9xSetJoypadLatch (bool latch)
{
	if (latch && !FLAG_LATCH)
	{
		// Latch going from 0 to 1 - reset read indices
		for (int n = 0; n < 2; n++)
			for (int j = 0; j < 2; j++)
				read_idx[n][j] = 0;
	}
	else if (!latch && FLAG_LATCH)
	{
		// Latch going from 1 to 0 - apply new controllers
		curcontrollers[0] = newcontrollers[0];
		curcontrollers[1] = newcontrollers[1];
	}

	FLAG_LATCH = latch;
}

// Post-increment a serial read index, clamping at 255 to prevent wrap-around.
// Returns the value before incrementing.  Once the index passes 16 (all button
// bits read), subsequent S9xReadJOYSERn calls return 1s — matching real hardware.
static inline uint8 IncreaseReadIdxPost(uint8 &var)
{
	uint8 oldval = var;
	if (var < 255)
		var++;
	return oldval;
}

// Handle reads from $4016 (port 0) or $4017 (port 1).
// If the latch is high, returns the current state of button B (bit 15) repeatedly.
// If the latch is low, returns joypad button bits serially (MSB first, one bit per
// read) via the read index.  After all 16 bits are read, returns 1.
// The upper bits of the return value preserve the open bus, with port 1 also
// setting bits 2-4 (matching SNES hardware).
// Accepts n as 0/1 (port index) or 0x4016/0x4017 (register address).
uint8 S9xReadJOYSERn (int n)
{
	int	i;

	if (n > 1)
		n -= 0x4016;
	assert(n == 0 || n == 1);

	uint8	bits = (OpenBus & ~3) | ((n == 1) ? 0x1c : 0);

	if (FLAG_LATCH)
	{
		switch (i = curcontrollers[n])
		{
			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				return (bits | ((joypad[i - JOYPAD0].buttons & 0x8000) ? 1 : 0));

			default:
				return (bits);
		}
	}
	else
	{
		switch (i = curcontrollers[n])
		{
			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				if (read_idx[n][0] >= 16)
				{
					IncreaseReadIdxPost(read_idx[n][0]);
					return (bits | 1);
				}
				else
					return (bits | ((joypad[i - JOYPAD0].buttons & (0x8000 >> IncreaseReadIdxPost(read_idx[n][0]))) ? 1 : 0));

			default:
				IncreaseReadIdxPost(read_idx[n][0]);
				return (bits);
		}
	}
}

// Perform the SNES auto-joypad read (triggered when bit 0 of $4200 is set).
// Strobes the latch (1 then 0), then copies each port's full 16-bit button
// state into the hardware auto-read registers ($4218-$421f) and marks the
// serial read index as fully consumed (16).
void S9xDoAutoJoypad (void)
{
	S9xSetJoypadLatch(1);
	S9xSetJoypadLatch(0);

	for (int n = 0; n < 2; n++)
	{
		switch (curcontrollers[n])
		{
			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				read_idx[n][0] = 16;
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, joypad[curcontrollers[n] - JOYPAD0].buttons);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;

			default:
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, 0);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;
		}
	}
}

// Called at the end of each frame.  Tracks whether the game read the joypad
// during this frame (pad_read) vs the previous frame (pad_read_last), which
// is used elsewhere to detect when a game is actively polling input.
void S9xControlEOF (void)
{
	pad_read_last = pad_read;
	pad_read      = false;
}

// Serialize controller state into a snapshot struct for save states.
// Stores: format version, serial read indices, all 8 joypad button states,
// and the pad_read flags.
void S9xControlPreSaveState (struct SControlSnapshot *s)
{
	memset(s, 0, sizeof(*s));
	s->ver = 7;  // Removed MP5 support

	for (int j = 0; j < 2; j++)
	{
		s->port1_read_idx[j] = read_idx[0][j];
		s->port2_read_idx[j] = read_idx[1][j];
	}

	// Save joypad button states (16 bytes for 8 joypads)
	for (int j = 0; j < 8; j++)
		memcpy(s->internal + j * 2, &joypad[j].buttons, 2);

	s->pad_read      = pad_read;
	s->pad_read_last = pad_read_last;
}

// Restore controller state from a save state snapshot.  Handles backward
// compatibility with older snapshot versions (v5 and earlier had different
// layouts with mouse/superscope/MP5 data, but the first 16 bytes were always
// the 8 joypad button states).  Also reconstructs FLAG_LATCH from $4016.
void S9xControlPostLoadState (struct SControlSnapshot *s)
{
	for (int j = 0; j < 2; j++)
	{
		read_idx[0][j] = s->port1_read_idx[j];
		read_idx[1][j] = s->port2_read_idx[j];
	}

	FLAG_LATCH = (Memory.FillRAM[0x4016] & 1) == 1;

	// Load joypad button states
	if (s->ver >= 7)
	{
		// New format without MP5
		for (int j = 0; j < 8; j++)
			memcpy(&joypad[j].buttons, s->internal + j * 2, 2);
	}
	else if (s->ver >= 6)
	{
		// Previous format (same as v7)
		for (int j = 0; j < 8; j++)
			memcpy(&joypad[j].buttons, s->internal + j * 2, 2);
	}
	else
	{
		// Old format - load first 16 bytes (joypad states)
		for (int j = 0; j < 8; j++)
			memcpy(&joypad[j].buttons, (char *) s->internal + j * 2, 2);
	}

	if (s->ver > 2)
	{
		pad_read      = s->pad_read;
		pad_read_last = s->pad_read_last;
	}
}

// Set the button state for a joypad slot (0-7) directly.
// This is the primary input entry point for frontends — call once per frame
// with a bitmask of SNES_*_MASK values from snes9x.h.
void S9xSetJoypadButtons (int pad, uint16 buttons)
{
	if (pad < 0 || pad > 7)
		return;

	joypad[pad].buttons = buttons;
}
