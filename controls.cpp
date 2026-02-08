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
#define MP5						(-1)
#define JOYPAD0					0
#define JOYPAD1					1
#define JOYPAD2					2
#define JOYPAD3					3
#define JOYPAD4					4
#define JOYPAD5					5
#define JOYPAD6					6
#define JOYPAD7					7
#define NUMCTLS					8 // This must be LAST

#define POLL_ALL				NUMCTLS

#define FLAG_IOBIT0				(Memory.FillRAM[0x4213] & 0x40)
#define FLAG_IOBIT1				(Memory.FillRAM[0x4213] & 0x80)
#define FLAG_IOBIT(n)			((n) ? (FLAG_IOBIT1) : (FLAG_IOBIT0))

bool8	pad_read = 0, pad_read_last = 0;
uint8	read_idx[2 /* ports */][2 /* per port */];

static struct
{
	uint16				buttons;
}	joypad[8];

static struct
{
	int8				pads[4];
}	mp5[2];

static bool8			FLAG_LATCH = FALSE;
static int32			curcontrollers[2] = { NONE,    NONE };
static int32			newcontrollers[2] = { JOYPAD0, NONE };

void S9xControlsReset (void)
{
	S9xControlsSoftReset();
}

void S9xControlsSoftReset (void)
{
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
			read_idx[i][j]=0;

	FLAG_LATCH = FALSE;

	curcontrollers[0] = newcontrollers[0];
	curcontrollers[1] = newcontrollers[1];
}

void S9xSetController (int port, enum controllers controller, int8 id1, int8 id2, int8 id3, int8 id4)
{
	if (port < 0 || port > 1)
		return;

	switch (controller)
	{
		case CTL_NONE:
			break;

		case CTL_JOYPAD:
			if (id1 < 0 || id1 > 7)
				break;

			newcontrollers[port] = JOYPAD0 + id1;
			return;

		case CTL_MP5:
			if (id1 < -1 || id1 > 7)
				break;
			if (id2 < -1 || id2 > 7)
				break;
			if (id3 < -1 || id3 > 7)
				break;
			if (id4 < -1 || id4 > 7)
				break;
			if (!Settings.MultiPlayer5Master)
			{
				S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select MP5: MultiPlayer5Master disabled");
				break;
			}

			newcontrollers[port] = MP5;
			mp5[port].pads[0] = (id1 < 0) ? NONE : JOYPAD0 + id1;
			mp5[port].pads[1] = (id2 < 0) ? NONE : JOYPAD0 + id2;
			mp5[port].pads[2] = (id3 < 0) ? NONE : JOYPAD0 + id3;
			mp5[port].pads[3] = (id4 < 0) ? NONE : JOYPAD0 + id4;
			return;

		default:
			fprintf(stderr, "Unknown controller type %d\n", controller);
			break;
	}

	newcontrollers[port] = NONE;
}

bool S9xVerifyControllers (void)
{
	bool	ret = false;
	int		port, i, used[NUMCTLS];

	for (i = 0; i < NUMCTLS; used[i++] = 0) ;

	for (port = 0; port < 2; port++)
	{
		switch (i = newcontrollers[port])
		{
			case MP5:
				if (!Settings.MultiPlayer5Master)
				{
					S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, "Cannot select MP5: MultiPlayer5Master disabled");
					newcontrollers[port] = NONE;
					ret = true;
					break;
				}

				for (i = 0; i < 4; i++)
				{
					if (mp5[port].pads[i] != NONE)
					{
						if (used[mp5[port].pads[i] - JOYPAD0]++ > 0)
						{
							char	buf[256];
							snprintf(buf, sizeof(buf), "Joypad%d used more than once! Disabling extra instances", mp5[port].pads[i] - JOYPAD0 + 1);
							S9xMessage(S9X_CONFIG_INFO, S9X_ERROR, buf);
							mp5[port].pads[i] = NONE;
							ret = true;
							break;
						}
					}
				}

				break;

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
					break;
				}

				break;

			default:
				break;
		}
	}

	return (ret);
}

static void do_polling (int mp)
{
	// Stub: old mapping system removed, pollmap is always empty.
	// New frontends use S9xSetJoypadButtons() directly instead of the mapping system.
	(void)mp;
}

void S9xSetJoypadLatch (bool latch)
{
	if (!latch && FLAG_LATCH)
	{
		// 1 written, 'plug in' new controllers now
		curcontrollers[0] = newcontrollers[0];
		curcontrollers[1] = newcontrollers[1];
	}

	if (latch && !FLAG_LATCH)
	{
		int	i;

		for (int n = 0; n < 2; n++)
		{
			for (int j = 0; j < 2; j++)
				read_idx[n][j] = 0;

			switch (i = curcontrollers[n])
			{
				case MP5:
					for (int j = 0, k; j < 4; ++j)
					{
						k = mp5[n].pads[j];
						if (k == NONE)
							continue;
						do_polling(k);
					}

					break;

				case JOYPAD0:
				case JOYPAD1:
				case JOYPAD2:
				case JOYPAD3:
				case JOYPAD4:
				case JOYPAD5:
				case JOYPAD6:
				case JOYPAD7:
					do_polling(i);
					break;

				default:
					break;
			}
		}
	}

	FLAG_LATCH = latch;
}

// prevent read_idx from overflowing (only latching resets it)
// otherwise $4016/7 reads will start returning input data again
static inline uint8 IncreaseReadIdxPost(uint8 &var)
{
	uint8 oldval = var;
	if (var < 255)
		var++;
	return oldval;
}

uint8 S9xReadJOYSERn (int n)
{
	int	i, j, r;

	if (n > 1)
		n -= 0x4016;
	assert(n == 0 || n == 1);

	uint8	bits = (OpenBus & ~3) | ((n == 1) ? 0x1c : 0);

	if (FLAG_LATCH)
	{
		switch (i = curcontrollers[n])
		{
			case MP5:
				return (bits | 2);

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
			case MP5:
				r = IncreaseReadIdxPost(read_idx[n][FLAG_IOBIT(n) ? 0 : 1]);
				j = FLAG_IOBIT(n) ? 0 : 2;

				for (i = 0; i < 2; i++, j++)
				{
					if (mp5[n].pads[j] == NONE)
						continue;
					if (r >= 16)
						bits |= 1 << i;
					else
						bits |= ((joypad[mp5[n].pads[j] - JOYPAD0].buttons & (0x8000 >> r)) ? 1 : 0) << i;
				}

				return (bits);

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

void S9xDoAutoJoypad (void)
{
	int	i, j;

	S9xSetJoypadLatch(1);
	S9xSetJoypadLatch(0);

	for (int n = 0; n < 2; n++)
	{
		switch (i = curcontrollers[n])
		{
			case MP5:
				j = FLAG_IOBIT(n) ? 0 : 2;
				for (i = 0; i < 2; i++, j++)
				{
					if (mp5[n].pads[j] == NONE)
						WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2 + i * 4, 0);
					else
						WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2 + i * 4, joypad[mp5[n].pads[j] - JOYPAD0].buttons);
				}

				read_idx[n][FLAG_IOBIT(n) ? 0 : 1] = 16;
				break;

			case JOYPAD0:
			case JOYPAD1:
			case JOYPAD2:
			case JOYPAD3:
			case JOYPAD4:
			case JOYPAD5:
			case JOYPAD6:
			case JOYPAD7:
				read_idx[n][0] = 16;
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, joypad[i - JOYPAD0].buttons);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;

			default:
				WRITE_WORD(Memory.FillRAM + 0x4218 + n * 2, 0);
				WRITE_WORD(Memory.FillRAM + 0x421c + n * 2, 0);
				break;
		}
	}
}

void S9xControlEOF (void)
{
	// Stub: turbo button processing removed with S9xApplyCommand
	// New frontends can implement turbo in their own input handling if needed

	(void)0;  // suppress unused warning

	pad_read_last = pad_read;
	pad_read      = false;
}

void S9xControlPreSaveState (struct SControlSnapshot *s)
{
	memset(s, 0, sizeof(*s));
	s->ver = 6;  // New version for simplified structure (removed turbo system)

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

void S9xControlPostLoadState (struct SControlSnapshot *s)
{
	for (int j = 0; j < 2; j++)
	{
		read_idx[0][j] = s->port1_read_idx[j];
		read_idx[1][j] = s->port2_read_idx[j];
	}

	FLAG_LATCH = (Memory.FillRAM[0x4016] & 1) == 1;

	// Load joypad button states
	if (s->ver >= 6)
	{
		// New simplified format (16 bytes for 8 joypads, no turbo state)
		for (int j = 0; j < 8; j++)
			memcpy(&joypad[j].buttons, s->internal + j * 2, 2);
	}
	else if (s->ver >= 5)
	{
		// Previous simplified format (same as v6)
		for (int j = 0; j < 8; j++)
			memcpy(&joypad[j].buttons, s->internal + j * 2, 2);
	}
	else
	{
		// Old format - try to load if compatible
		// Note: Loading old savestates with mouse/superscope/justifier data
		// will only restore the joypad states, which is typically sufficient.
		int i = 0;

		// Load 8 joypad button states (first 16 bytes of old internal)
		for (int j = 0; j < 8; j++)
		{
			memcpy(&joypad[j].buttons, (char *) s->internal + i, 2);
			i += 2;
		}

		// Skip over mouse, superscope, justifier, and MP5 data
		// (not used in new simplified version)
	}

	if (s->ver > 2)
	{
		pad_read      = s->pad_read;
		pad_read_last = s->pad_read_last;
	}
}

void S9xSetJoypadButtons (int pad, uint16 buttons)
{
	if (pad < 0 || pad > 7)
		return;

	joypad[pad].buttons = buttons;
}
