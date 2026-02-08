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
	uint16				turbos;
	uint16				toggleturbo;
	uint16				togglestick;
	uint8				turbo_ct;
}	joypad[8];

static struct
{
	int8				pads[4];
}	mp5[2];

static uint8			turbo_time = 1;
static bool8			FLAG_LATCH = FALSE;
static int32			curcontrollers[2] = { NONE,    NONE };
static int32			newcontrollers[2] = { JOYPAD0, NONE };
static char				buf[256];

// Command enum for S9xButtonCommand case
#define THE_COMMANDS \
	S(ClipWindows), \
	S(Debugger), \
	S(DecEmuTurbo), \
	S(DecFrameRate), \
	S(DecFrameTime), \
	S(DecTurboSpeed), \
	S(EmuTurbo), \
	S(ExitEmu), \
	S(IncEmuTurbo), \
	S(IncFrameRate), \
	S(IncFrameTime), \
	S(IncTurboSpeed), \
	S(LoadFreezeFile), \
	S(LoadOopsFile), \
	S(Pause), \
	S(QuickLoad000), \
	S(QuickLoad001), \
	S(QuickLoad002), \
	S(QuickLoad003), \
	S(QuickLoad004), \
	S(QuickLoad005), \
	S(QuickLoad006), \
	S(QuickLoad007), \
	S(QuickLoad008), \
	S(QuickLoad009), \
	S(QuickLoad010), \
	S(QuickSave000), \
	S(QuickSave001), \
	S(QuickSave002), \
	S(QuickSave003), \
	S(QuickSave004), \
	S(QuickSave005), \
	S(QuickSave006), \
	S(QuickSave007), \
	S(QuickSave008), \
	S(QuickSave009), \
	S(QuickSave010), \
	S(Reset), \
	S(SaveFreezeFile), \
	S(SaveSPC), \
	S(Screenshot), \
	S(SoftReset), \
	S(SoundChannel0), \
	S(SoundChannel1), \
	S(SoundChannel2), \
	S(SoundChannel3), \
	S(SoundChannel4), \
	S(SoundChannel5), \
	S(SoundChannel6), \
	S(SoundChannel7), \
	S(SoundChannelsOn), \
	S(SwapJoypads), \
	S(ToggleBG0), \
	S(ToggleBG1), \
	S(ToggleBG2), \
	S(ToggleBG3), \
	S(ToggleBackdrop), \
	S(ToggleEmuTurbo), \
	S(ToggleSprites), \
	S(ToggleTransparency)

#define S(x)	x

enum command_numbers
{
	THE_COMMANDS,
	LAST_COMMAND
};

#undef S
#undef THE_COMMANDS

static void DisplayStateChange (const char *, bool8);

static void DisplayStateChange (const char *str, bool8 on)
{
	snprintf(buf, sizeof(buf), "%s: %s", str, on ? "on":"off");
	S9xSetInfoString(buf);
}

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

void S9xApplyCommand (s9xcommand_t cmd, int16 data1, int16 data2)
{
	int	i;

	switch (cmd.type)
	{

		case S9xButtonJoypad:
			if (cmd.button.joypad.toggle)
			{
				if (!data1)
					return;

				uint16	r = cmd.button.joypad.buttons;

				if (cmd.button.joypad.turbo)	joypad[cmd.button.joypad.idx].toggleturbo ^= r;
				if (cmd.button.joypad.sticky)	joypad[cmd.button.joypad.idx].togglestick ^= r;
			}
			else
			{
				uint16	r, s, t, st;

				r = cmd.button.joypad.buttons;
				st = r & joypad[cmd.button.joypad.idx].togglestick & joypad[cmd.button.joypad.idx].toggleturbo;
				r ^= st;
				t  = r & joypad[cmd.button.joypad.idx].toggleturbo;
				r ^= t;
				s  = r & joypad[cmd.button.joypad.idx].togglestick;
				r ^= s;

				if (cmd.button.joypad.turbo && cmd.button.joypad.sticky)
				{
					uint16	x = r; r = st; st = x;
					x = s; s = t; t = x;
				}
				else if (cmd.button.joypad.turbo)
				{
					uint16	x = r; r = t; t = x;
					x = s; s = st; st = x;
				}
				else if (cmd.button.joypad.sticky)
				{
					uint16	x = r; r = s; s = x;
					x = t; t = st; st = x;
				}

				if (data1)
				{
					joypad[cmd.button.joypad.idx].buttons |= r;
					joypad[cmd.button.joypad.idx].turbos  |= t;
					joypad[cmd.button.joypad.idx].buttons ^= s;
					joypad[cmd.button.joypad.idx].buttons &= ~(joypad[cmd.button.joypad.idx].turbos & st);
					joypad[cmd.button.joypad.idx].turbos  ^= st;
				}
				else
				{
					joypad[cmd.button.joypad.idx].buttons &= ~r;
					joypad[cmd.button.joypad.idx].buttons &= ~(joypad[cmd.button.joypad.idx].turbos & t);
					joypad[cmd.button.joypad.idx].turbos  &= ~t;
				}
			}

			return;

		case S9xButtonCommand:
			if (((enum command_numbers) cmd.button.command) >= LAST_COMMAND)
			{
				fprintf(stderr, "Unknown command %04x\n", cmd.button.command);
				return;
			}

			if (!data1)
			{
				switch (i = cmd.button.command)
				{
					case EmuTurbo:
						break;
				}
			}
			else
			{
				switch ((enum command_numbers) (i = cmd.button.command))
				{
					case ExitEmu:
						S9xExit();
						break;

					case Reset:
						S9xReset();
						break;

					case SoftReset:
						S9xSoftReset();
						break;

					case EmuTurbo:
						break;

					case ToggleEmuTurbo:
						break;

					case ClipWindows:
						Settings.DisableGraphicWindows = !Settings.DisableGraphicWindows;
						DisplayStateChange("Graphic clip windows", !Settings.DisableGraphicWindows);
						break;

					case Debugger:
						// Debugger removed - not supported in new frontends
						break;

					case IncFrameRate:
						if (Settings.SkipFrames == AUTO_FRAMERATE)
							Settings.SkipFrames = 1;
						else
						if (Settings.SkipFrames < 10)
							Settings.SkipFrames++;

						if (Settings.SkipFrames == AUTO_FRAMERATE)
							S9xSetInfoString("Auto frame skip");
						else
						{
							sprintf(buf, "Frame skip: %d", Settings.SkipFrames - 1);
							S9xSetInfoString(buf);
						}

						break;

					case DecFrameRate:
						if (Settings.SkipFrames <= 1)
							Settings.SkipFrames = AUTO_FRAMERATE;
						else
						if (Settings.SkipFrames != AUTO_FRAMERATE)
							Settings.SkipFrames--;

						if (Settings.SkipFrames == AUTO_FRAMERATE)
							S9xSetInfoString("Auto frame skip");
						else
						{
							sprintf(buf, "Frame skip: %d", Settings.SkipFrames - 1);
							S9xSetInfoString(buf);
						}

						break;

					case IncEmuTurbo:
						break;

					case DecEmuTurbo:
						break;

					case IncFrameTime: // Increase emulated frame time by 1ms
						Settings.FrameTime += 1000;
						sprintf(buf, "Emulated frame time: %dms", Settings.FrameTime / 1000);
						S9xSetInfoString(buf);
						break;

					case DecFrameTime: // Decrease emulated frame time by 1ms
						if (Settings.FrameTime >= 1000)
							Settings.FrameTime -= 1000;
						sprintf(buf, "Emulated frame time: %dms", Settings.FrameTime / 1000);
						S9xSetInfoString(buf);
						break;

					case IncTurboSpeed:
						if (turbo_time >= 120)
							break;
						turbo_time++;
						sprintf(buf, "Turbo speed: %d", turbo_time);
						S9xSetInfoString(buf);
						break;

					case DecTurboSpeed:
						if (turbo_time <= 1)
							break;
						turbo_time--;
						sprintf(buf, "Turbo speed: %d", turbo_time);
						S9xSetInfoString(buf);
						break;

					case LoadFreezeFile:
						break;

					case SaveFreezeFile:
						break;

					case LoadOopsFile:
					{
						std::string filename = S9xGetFilename("oops", SNAPSHOT_DIR);

						if (S9xUnfreezeGame(filename.c_str()))
						{
							snprintf(buf, 256, "%.240s.oops loaded", S9xBasename(Memory.ROMFilename).c_str());
							S9xSetInfoString(buf);
						}
						else
							S9xMessage(S9X_ERROR, S9X_FREEZE_FILE_NOT_FOUND, "Oops file not found");

						break;
					}

					case Pause:
						Settings.Paused = !Settings.Paused;
						DisplayStateChange("Pause", Settings.Paused);
						break;

					case QuickLoad000:
					case QuickLoad001:
					case QuickLoad002:
					case QuickLoad003:
					case QuickLoad004:
					case QuickLoad005:
					case QuickLoad006:
					case QuickLoad007:
					case QuickLoad008:
					case QuickLoad009:
					case QuickLoad010:
					{
						std::string ext = std::to_string(i - QuickLoad000);
						while (ext.length() < 3)
							ext = '0' + ext;

						auto filename = S9xGetFilename(ext, SNAPSHOT_DIR);

						if (S9xUnfreezeGame(filename.c_str()))
						{
							snprintf(buf, 256, "%s loaded", S9xBasename(filename).c_str());
							S9xSetInfoString(buf);
						}
						else
							S9xMessage(S9X_ERROR, S9X_FREEZE_FILE_NOT_FOUND, "Freeze file not found");

						break;
					}

					case QuickSave000:
					case QuickSave001:
					case QuickSave002:
					case QuickSave003:
					case QuickSave004:
					case QuickSave005:
					case QuickSave006:
					case QuickSave007:
					case QuickSave008:
					case QuickSave009:
					case QuickSave010:
					{
						std::string ext = std::to_string(i - QuickSave000);
						while (ext.length() < 3)
							ext = '0' + ext;

						auto filename = S9xGetFilename(ext, SNAPSHOT_DIR);

						snprintf(buf, 256, "%s saved", S9xBasename(filename).c_str());
						S9xSetInfoString(buf);

						S9xFreezeGame(filename.c_str());
						break;
					}

					case SaveSPC:
						S9xDumpSPCSnapshot();
						break;

					case Screenshot:
						break;

					case SoundChannel0:
					case SoundChannel1:
					case SoundChannel2:
					case SoundChannel3:
					case SoundChannel4:
					case SoundChannel5:
					case SoundChannel6:
					case SoundChannel7:
						S9xToggleSoundChannel(i - SoundChannel0);
						sprintf(buf, "Sound channel %d toggled", i - SoundChannel0);
						S9xSetInfoString(buf);
						break;

					case SoundChannelsOn:
						S9xToggleSoundChannel(8);
						S9xSetInfoString("All sound channels on");
						break;

					case ToggleBackdrop:
						switch (Settings.ForcedBackdrop)
						{
						case 0:
							Settings.ForcedBackdrop = 0xf81f;
							break;
						case 0xf81f:
							Settings.ForcedBackdrop = 0x07e0;
							break;
						case 0x07e0:
							Settings.ForcedBackdrop = 0x07ff;
							break;
						default:
							Settings.ForcedBackdrop = 0;
							break;
						}
						sprintf(buf, "Setting backdrop to 0x%04x", Settings.ForcedBackdrop);
						S9xSetInfoString(buf);
						break;

					case ToggleBG0:
						Settings.BG_Forced ^= 1;
						DisplayStateChange("BG#0", !(Settings.BG_Forced & 1));
						break;

					case ToggleBG1:
						Settings.BG_Forced ^= 2;
						DisplayStateChange("BG#1", !(Settings.BG_Forced & 2));
						break;

					case ToggleBG2:
						Settings.BG_Forced ^= 4;
						DisplayStateChange("BG#2", !(Settings.BG_Forced & 4));
						break;

					case ToggleBG3:
						Settings.BG_Forced ^= 8;
						DisplayStateChange("BG#3", !(Settings.BG_Forced & 8));
						break;

					case ToggleSprites:
						Settings.BG_Forced ^= 16;
						DisplayStateChange("Sprites", !(Settings.BG_Forced & 16));
						break;

					case ToggleTransparency:
						Settings.Transparency = !Settings.Transparency;
						DisplayStateChange("Transparency effects", Settings.Transparency);
						break;

					case SwapJoypads:
						if ((curcontrollers[0] != NONE && !(curcontrollers[0] >= JOYPAD0 && curcontrollers[0] <= JOYPAD7)))
						{
							S9xSetInfoString("Cannot swap pads: port 1 is not a joypad");
							break;
						}

						if ((curcontrollers[1] != NONE && !(curcontrollers[1] >= JOYPAD0 && curcontrollers[1] <= JOYPAD7)))
						{
							S9xSetInfoString("Cannot swap pads: port 2 is not a joypad");
							break;
						}

						newcontrollers[1] = curcontrollers[0];
						newcontrollers[0] = curcontrollers[1];

						strcpy(buf, "Swap pads: P1=");
						i = 14;
						if (newcontrollers[0] == NONE)
						{
							strcpy(buf + i, "<none>");
							i += 6;
						}
						else
						{
							sprintf(buf + i, "Joypad%d", newcontrollers[0] - JOYPAD0 + 1);
							i += 7;
						}

						strcpy(buf + i, " P2=");
						i += 4;
						if (newcontrollers[1] == NONE)
							strcpy(buf + i, "<none>");
						else
							sprintf(buf + i, "Joypad%d", newcontrollers[1] - JOYPAD0 + 1);

						S9xSetInfoString(buf);
						break;

					case LAST_COMMAND:
						break;
				}
			}

			return;

		case S9xAxisJoypad:
		{
			uint16	pos, neg;

			switch (cmd.joypad_axis.axis)
			{
				case 0: neg = SNES_LEFT_MASK;	pos = SNES_RIGHT_MASK;	break;
				case 1: neg = SNES_UP_MASK;		pos = SNES_DOWN_MASK;	break;
				case 2: neg = SNES_Y_MASK;		pos = SNES_A_MASK;		break;
				case 3: neg = SNES_X_MASK;		pos = SNES_B_MASK;		break;
				case 4: neg = SNES_TL_MASK;		pos = SNES_TR_MASK;		break;
				default: return;
			}

			if (cmd.joypad_axis.invert)
				data1 = -data1;

			uint16	p, r;

			p = r = 0;
			if (data1 >  ((cmd.joypad_axis.threshold + 1) *  127))
				p |= pos;
			else
				r |= pos;

			if (data1 <= ((cmd.joypad_axis.threshold + 1) * -127))
				p |= neg;
			else
				r |= neg;

			joypad[cmd.joypad_axis.idx].buttons |= p;
			joypad[cmd.joypad_axis.idx].buttons &= ~r;
			joypad[cmd.joypad_axis.idx].turbos  &= ~(p | r);

			return;
		}

		case S9xButtonPort:
		case S9xAxisPort:
		case S9xPointerPort:
			S9xHandlePortCommand(cmd, data1, data2);
			return;

		default:
			fprintf(stderr, "WARNING: Unknown command type %d\n", cmd.type);
			return;
	}
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
	int	i, j;

	for (int n = 0; n < 2; n++)
	{
		switch (i = curcontrollers[n])
		{
			case MP5:
				for (j = 0; j < 4; ++j)
				{
					i = mp5[n].pads[j];
					if (i == NONE)
						continue;

					if (++joypad[i - JOYPAD0].turbo_ct >= turbo_time)
					{
						joypad[i - JOYPAD0].turbo_ct = 0;
						joypad[i - JOYPAD0].buttons ^= joypad[i - JOYPAD0].turbos;
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
				if (++joypad[i - JOYPAD0].turbo_ct >= turbo_time)
				{
					joypad[i - JOYPAD0].turbo_ct = 0;
					joypad[i - JOYPAD0].buttons ^= joypad[i - JOYPAD0].turbos;
				}

				break;

			default:
				break;
		}
	}

	pad_read_last = pad_read;
	pad_read      = false;
}

void S9xControlPreSaveState (struct SControlSnapshot *s)
{
	memset(s, 0, sizeof(*s));
	s->ver = 5;  // New version for simplified structure

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
	if (s->ver >= 5)
	{
		// New simplified format (16 bytes for 8 joypads)
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

		// Skip over mouse, superscope, justifier data
		i += (2 * (1 + 1 + 2 + 2 + 2 + 2 + 1));  // Skip mouse data
		i += (2 + 2 + 1 + 1 + 1);                   // Skip superscope data
		i += (2 + 2 + 2 + 2 + 1 + 1 + 1);           // Skip justifier data

		// Load MP5 configuration if present
		if (s->ver > 1)
		{
			for (int j = 0; j < 2; j++)
				for (int k = 0; k < 2; k++)
					memcpy(&mp5[j].pads[k], (char *) s->internal + i, k < 2 ? sizeof(int8) : 0);
		}
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

