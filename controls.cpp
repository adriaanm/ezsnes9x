/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include <map>
#include <set>
#include <vector>
#include <string>
#include <algorithm>
#include <assert.h>

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "snapshot.h"
#include "controls.h"
#include "display.h"

using namespace	std;

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

#define MAP_UNKNOWN				(-1)
#define MAP_NONE				0
#define MAP_BUTTON				1
#define MAP_AXIS				2
#define MAP_POINTER				3

#define FLAG_IOBIT0				(Memory.FillRAM[0x4213] & 0x40)
#define FLAG_IOBIT1				(Memory.FillRAM[0x4213] & 0x80)
#define FLAG_IOBIT(n)			((n) ? (FLAG_IOBIT1) : (FLAG_IOBIT0))

bool8	pad_read = 0, pad_read_last = 0;
uint8	read_idx[2 /* ports */][2 /* per port */];

struct exemulti
{
	int32				pos;
	bool8				data1;
	s9xcommand_t		*script;
};

static struct
{
	int16				x, y;
	int16				V_adj;
	bool8				V_var;
	int16				H_adj;
	bool8				H_var;
	bool8				mapped;
}	pseudopointer[8];

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

static set<struct exemulti *>		exemultis;
static set<uint32>					pollmap[NUMCTLS + 1];
static map<uint32, s9xcommand_t>	keymap;
static vector<s9xcommand_t *>		multis;
static uint8						turbo_time;
static uint8						pseudobuttons[256];
static bool8						FLAG_LATCH = FALSE;
static int32						curcontrollers[2] = { NONE,    NONE };
static int32						newcontrollers[2] = { JOYPAD0, NONE };
static char							buf[256];

static const char	*speed_names[4] =
{
	"Var",
	"Slow",
	"Med",
	"Fast"
};

static const int	ptrspeeds[4] = { 1, 1, 4, 8 };

// Note: these should be in asciibetical order!
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
	S(ToggleTransparency) \

#define S(x)	x

enum command_numbers
{
	THE_COMMANDS,
	LAST_COMMAND
};

#undef S
#define S(x)	#x

static const char	*command_names[LAST_COMMAND + 1] =
{
	THE_COMMANDS,
	NULL
};

#undef S
#undef THE_COMMANDS

static void DisplayStateChange (const char *, bool8);
static int maptype (int);
static bool strless (const char *, const char *);
static int findstr (const char *, const char **, int);
static int get_threshold (const char **);
static const char * maptypename (int);
static int32 ApplyMulti (s9xcommand_t *, int32, int16);
static void do_polling (int);


static string& operator += (string &s, int i)
{
	snprintf(buf, sizeof(buf), "%d", i);
	s.append(buf);
	return (s);
}

static string& operator += (string &s, double d)
{
	snprintf(buf, sizeof(buf), "%g", d);
	s.append(buf);
	return (s);
}

static void DisplayStateChange (const char *str, bool8 on)
{
	snprintf(buf, sizeof(buf), "%s: %s", str, on ? "on":"off");
	S9xSetInfoString(buf);
}

static int maptype (int t)
{
	switch (t)
	{
		case S9xNoMapping:
			return (MAP_NONE);

		case S9xButtonJoypad:
		case S9xButtonCommand:
		case S9xButtonPseudopointer:
		case S9xButtonPort:
		case S9xButtonMulti:
			return (MAP_BUTTON);

		case S9xAxisJoypad:
		case S9xAxisPseudopointer:
		case S9xAxisPseudobuttons:
		case S9xAxisPort:
			return (MAP_AXIS);

		case S9xPointer:
		case S9xPointerPort:
			return (MAP_POINTER);

		default:
			return (MAP_UNKNOWN);
	}
}

void S9xControlsReset (void)
{
	S9xControlsSoftReset();
}

void S9xControlsSoftReset (void)
{
	for (set<struct exemulti *>::iterator it = exemultis.begin(); it != exemultis.end(); it++)
		delete *it;
	exemultis.clear();

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

char * S9xGetCommandName (s9xcommand_t command)
{
	string	s;
	char	c;

	switch (command.type)
	{
		case S9xButtonJoypad:
			if (command.button.joypad.buttons == 0)
				return (strdup("None"));
			if (command.button.joypad.buttons & 0x000f)
				return (strdup("None"));

			s = "Joypad";
			s += command.button.joypad.idx + 1;

			c = ' ';
			if (command.button.joypad.toggle)	{ if (c) s += c; s += "Toggle"; c = 0; }
			if (command.button.joypad.sticky)	{ if (c) s += c; s += "Sticky"; c = 0; }
			if (command.button.joypad.turbo )	{ if (c) s += c; s += "Turbo";  c = 0; }

			c = ' ';
			if (command.button.joypad.buttons & SNES_UP_MASK    )	{ s += c; s += "Up";     c = '+'; }
			if (command.button.joypad.buttons & SNES_DOWN_MASK  )	{ s += c; s += "Down";   c = '+'; }
			if (command.button.joypad.buttons & SNES_LEFT_MASK  )	{ s += c; s += "Left";   c = '+'; }
			if (command.button.joypad.buttons & SNES_RIGHT_MASK )	{ s += c; s += "Right";  c = '+'; }
			if (command.button.joypad.buttons & SNES_A_MASK     )	{ s += c; s += "A";      c = '+'; }
			if (command.button.joypad.buttons & SNES_B_MASK     )	{ s += c; s += "B";      c = '+'; }
			if (command.button.joypad.buttons & SNES_X_MASK     )	{ s += c; s += "X";      c = '+'; }
			if (command.button.joypad.buttons & SNES_Y_MASK     )	{ s += c; s += "Y";      c = '+'; }
			if (command.button.joypad.buttons & SNES_TL_MASK    )	{ s += c; s += "L";      c = '+'; }
			if (command.button.joypad.buttons & SNES_TR_MASK    )	{ s += c; s += "R";      c = '+'; }
			if (command.button.joypad.buttons & SNES_START_MASK )	{ s += c; s += "Start";  c = '+'; }
			if (command.button.joypad.buttons & SNES_SELECT_MASK)	{ s += c; s += "Select"; c = '+'; }

			break;

		case S9xButtonCommand:
			if (command.button.command >= LAST_COMMAND)
				return (strdup("None"));

			return (strdup(command_names[command.button.command]));

		case S9xPointer:
			return (strdup("None"));

		case S9xButtonPseudopointer:
			if (!command.button.pointer.UD && !command.button.pointer.LR)
				return (strdup("None"));
			if (command.button.pointer.UD == -2 || command.button.pointer.LR == -2)
				return (strdup("None"));

			s = "ButtonToPointer ";
			s += command.button.pointer.idx + 1;

			if (command.button.pointer.UD)	s += (command.button.pointer.UD == 1) ? 'd' : 'u';
			if (command.button.pointer.LR)	s += (command.button.pointer.LR == 1) ? 'r' : 'l';

			s += " ";
			s += speed_names[command.button.pointer.speed_type];

			break;

		case S9xAxisJoypad:
			s = "Joypad";
			s += command.axis.joypad.idx + 1;
			s += " Axis ";

			switch (command.axis.joypad.axis)
			{
				case 0:	s += (command.axis.joypad.invert ? "Right/Left" : "Left/Right");	break;
				case 1:	s += (command.axis.joypad.invert ? "Down/Up"    : "Up/Down"   );	break;
				case 2:	s += (command.axis.joypad.invert ? "A/Y"        : "Y/A"       );	break;
				case 3:	s += (command.axis.joypad.invert ? "B/X"        : "X/B"       );	break;
				case 4:	s += (command.axis.joypad.invert ? "R/L"        : "L/R"       );	break;
				default:	return (strdup("None"));
			}

			s += " T=";
			s += int((command.axis.joypad.threshold + 1) * 1000 / 256) / 10.0;
			s += "%";

			break;

		case S9xAxisPseudopointer:
			s = "AxisToPointer ";
			s += command.axis.pointer.idx + 1;
			s += command.axis.pointer.HV ? 'v' : 'h';
			s += " ";

			if (command.axis.pointer.invert)	s += "-";

			s += speed_names[command.axis.pointer.speed_type];

			break;

		case S9xAxisPseudobuttons:
			s = "AxisToButtons ";
			s += command.axis.button.negbutton;
			s += "/";
			s += command.axis.button.posbutton;
			s += " T=";
			s += int((command.axis.button.threshold + 1) * 1000 / 256) / 10.0;
			s += "%";

			break;

		case S9xButtonPort:
		case S9xAxisPort:
		case S9xPointerPort:
			return (strdup("BUG: Port should have handled this instead of calling S9xGetCommandName()"));

		case S9xNoMapping:
			return (strdup("None"));

		case S9xButtonMulti:
		{
			if (command.button.multi_idx >= (int) multis.size())
				return (strdup("None"));

			s = "{";
			if (multis[command.button.multi_idx]->multi_press)	s = "+{";

			bool	sep = false;

			for (s9xcommand_t *m = multis[command.button.multi_idx]; m->multi_press != 3; m++)
			{
				if (m->type == S9xNoMapping)
				{
					s += ";";
					sep = false;
				}
				else
				{
					if (sep)					s += ",";
					if (m->multi_press == 1)	s += "+";
					if (m->multi_press == 2)	s += "-";

					s += S9xGetCommandName(*m);
					sep = true;
				}
			}

			s += "}";

			break;
		}

		default:
			return (strdup("BUG: Unknown command type"));
	}

	return (strdup(s.c_str()));
}

static bool strless (const char *a, const char *b)
{
	return (strcmp(a, b) < 0);
}

static int findstr (const char *needle, const char **haystack, int numstr)
{
	const char	**r;

	r = lower_bound(haystack, haystack + numstr, needle, strless);
	if (r >= haystack + numstr || strcmp(needle, *r))
		return (-1);

	return (r - haystack);
}

static int get_threshold (const char **ss)
{
	const char	*s = *ss;
	int			i;

	if (s[0] != 'T' || s[1] != '=')
		return (-1);

	s += 2;
	i = 0;

	if (s[0] == '0')
	{
		if (s[1] != '.')
			return (-1);

		s++;
	}
	else
	{
		do
		{
			if (*s < '0' || *s > '9')
				return (-1);

			i = i * 10 + 10 * (*s - '0');
			if (i > 1000)
				return (-1);

			s++;
		}
		while (*s != '.' && *s != '%');
	}

	if (*s == '.')
	{
		if (s[1] < '0' || s[1] > '9' || s[2] != '%')
			return (-1);

		i += s[1] - '0';
	}

	if (i > 1000)
		return (-1);

	*ss = s;

	return (i);
}

s9xcommand_t S9xGetCommandT (const char *name)
{
	s9xcommand_t	cmd;
	int				i, j;
	const char		*s;

	memset(&cmd, 0, sizeof(cmd));
	cmd.type         = S9xBadMapping;
	cmd.multi_press  = 0;
	cmd.button_norpt = 0;

	if (!strcmp(name, "None"))
		cmd.type = S9xNoMapping;
	else if (!strncmp(name, "Joypad", 6))
	{
		if (name[6] < '1' || name[6] > '8' || name[7] != ' ')
			return (cmd);

		if (!strncmp(name + 8, "Axis ", 5))
		{
			cmd.axis.joypad.idx = name[6] - '1';
			s = name + 13;

			if (!strncmp(s, "Left/Right ", 11))	{ j = 0; i = 0; s += 11; }
			else if (!strncmp(s, "Right/Left ", 11))	{ j = 0; i = 1; s += 11; }
			else if (!strncmp(s, "Up/Down ",     8))	{ j = 1; i = 0; s +=  8; }
			else if (!strncmp(s, "Down/Up ",     8))	{ j = 1; i = 1; s +=  8; }
			else if (!strncmp(s, "Y/A ",         4))	{ j = 2; i = 0; s +=  4; }
			else if (!strncmp(s, "A/Y ",         4))	{ j = 2; i = 1; s +=  4; }
			else if (!strncmp(s, "X/B ",         4))	{ j = 3; i = 0; s +=  4; }
			else if (!strncmp(s, "B/X ",         4))	{ j = 3; i = 1; s +=  4; }
			else if (!strncmp(s, "L/R ",         4))	{ j = 4; i = 0; s +=  4; }
			else if (!strncmp(s, "R/L ",         4))	{ j = 4; i = 1; s +=  4; }
			else
				return (cmd);

			cmd.axis.joypad.axis      = j;
			cmd.axis.joypad.invert    = i;
			i = get_threshold(&s);
			if (i < 0)
				return (cmd);
			cmd.axis.joypad.threshold = (i - 1) * 256 / 1000;

			cmd.type = S9xAxisJoypad;
		}
		else
		{
			cmd.button.joypad.idx = name[6] - '1';
			s = name + 8;
			i = 0;

			if ((cmd.button.joypad.toggle = strncmp(s, "Toggle", 6) ? 0 : 1))	s += i = 6;
			if ((cmd.button.joypad.sticky = strncmp(s, "Sticky", 6) ? 0 : 1))	s += i = 6;
			if ((cmd.button.joypad.turbo  = strncmp(s, "Turbo",  5) ? 0 : 1))	s += i = 5;

			if (cmd.button.joypad.toggle && !(cmd.button.joypad.sticky || cmd.button.joypad.turbo))
				return (cmd);

			if (i)
			{
				if (*s != ' ')
					return (cmd);
				s++;
			}

			i = 0;

			if (!strncmp(s, "Up",     2))	{ i |= SNES_UP_MASK;     s += 2; if (*s == '+') s++; }
			if (!strncmp(s, "Down",   4))	{ i |= SNES_DOWN_MASK;   s += 4; if (*s == '+') s++; }
			if (!strncmp(s, "Left",   4))	{ i |= SNES_LEFT_MASK;   s += 4; if (*s == '+') s++; }
			if (!strncmp(s, "Right",  5))	{ i |= SNES_RIGHT_MASK;  s += 5; if (*s == '+') s++; }

			if (*s == 'A')	{ i |= SNES_A_MASK;  s++; if (*s == '+') s++; }
			if (*s == 'B')	{ i |= SNES_B_MASK;  s++; if (*s == '+') s++; }
			if (*s == 'X')	{ i |= SNES_X_MASK;  s++; if (*s == '+') s++; }
			if (*s == 'Y')	{ i |= SNES_Y_MASK;  s++; if (*s == '+') s++; }
			if (*s == 'L')	{ i |= SNES_TL_MASK; s++; if (*s == '+') s++; }
			if (*s == 'R')	{ i |= SNES_TR_MASK; s++; if (*s == '+') s++; }

			if (!strncmp(s, "Start",  5))	{ i |= SNES_START_MASK;  s += 5; if (*s == '+') s++; }
			if (!strncmp(s, "Select", 6))	{ i |= SNES_SELECT_MASK; s += 6; }

			if (i == 0 || *s != 0 || *(s - 1) == '+')
				return (cmd);

			cmd.button.joypad.buttons = i;

			cmd.type = S9xButtonJoypad;
		}
	}
	else
	if (!strncmp(name, "ButtonToPointer ", 16))
	{
		if (name[16] < '1' || name[16] > '8')
			return (cmd);

		cmd.button.pointer.idx = name[16] - '1';
		s = name + 17;
		i = 0;

		if ((cmd.button.pointer.UD = (*s == 'u' ? -1 : (*s == 'd' ? 1 : 0))))	s += i = 1;
		if ((cmd.button.pointer.LR = (*s == 'l' ? -1 : (*s == 'r' ? 1 : 0))))	s += i = 1;

		if (i == 0 || *(s++) != ' ')
			return (cmd);

		for (i = 0; i < 4; i++)
			if (!strcmp(s, speed_names[i]))
				break;
		if (i > 3)
			return (cmd);

		cmd.button.pointer.speed_type = i;

		cmd.type = S9xButtonPseudopointer;
	}
	else
	if (!strncmp(name, "AxisToPointer ", 14))
	{
		if (name[14] < '1' || name[14] > '8')
			return (cmd);

		cmd.axis.pointer.idx = name[14] - '1';
		s= name + 15;
		i = 0;

		if (*s == 'h')
			cmd.axis.pointer.HV = 0;
		else if (*s == 'v')
			cmd.axis.pointer.HV = 1;
		else
			return (cmd);

		if (s[1] != ' ')
			return (cmd);

		s += 2;
		if ((cmd.axis.pointer.invert = *s == '-'))
			s++;

		for (i = 0; i < 4; i++)
			if (!strcmp(s, speed_names[i]))
				break;
		if (i > 3)
			return (cmd);

		cmd.axis.pointer.speed_type = i;

		cmd.type = S9xAxisPseudopointer;
	}
	else
	if (!strncmp(name, "AxisToButtons ", 14))
	{
		s = name + 14;

		if (s[0] == '0')
		{
			if (s[1] != '/')
				return (cmd);

			cmd.axis.button.negbutton = 0;
			s += 2;
		}
		else
		{
			i = 0;
			do
			{
				if (*s < '0' || *s > '9')
					return (cmd);

				i = i * 10 + *s - '0';
				if (i > 255)
					return (cmd);
			}
			while (*++s != '/');

			cmd.axis.button.negbutton = i;
			s++;
		}

		if (s[0] == '0')
		{
			if (s[1] != ' ')
				return (cmd);

			cmd.axis.button.posbutton = 0;
			s += 2;
		}
		else
		{
			i = 0;
			do
			{
				if (*s < '0' || *s > '9')
					return (cmd);

				i = i * 10 + *s - '0';
				if (i > 255)
					return (cmd);
			}
			while (*++s != ' ');

			cmd.axis.button.posbutton = i;
			s++;
		}

		i = get_threshold(&s);
		if (i < 0)
			return (cmd);
		cmd.axis.button.threshold = (i - 1) * 256 / 1000;

		cmd.type = S9xAxisPseudobuttons;
	}
	else
	if (!strncmp(name, "MULTI#", 6))
	{
		i = strtol(name + 6, (char **) &s, 10);
		if (s != NULL && *s != '\0')
			return (cmd);
		if (i >= (int) multis.size())
			return (cmd);

		cmd.button.multi_idx = i;
		cmd.type = S9xButtonMulti;
	}
	else
	if (((name[0] == '+' && name[1] == '{') || name[0] == '{') && name[strlen(name) - 1] == '}')
	{
		if (multis.size() > 2147483640)
		{
			fprintf(stderr, "Too many multis!");
			return (cmd);
		}

		string	x;
		int		n;

		j = 2;
		for (i = (name[0] == '+') ? 2 : 1; name[i] != '\0'; i++)
		{
			if (name[i] == ',' || name[i] == ';')
			{
				if (name[i] == ';')
					j++;
				if (++j > 2147483640)
				{
					fprintf(stderr, "Multi too long!");
					return (cmd);
				}
			}

			if (name[i] == '{')
				return (cmd);
		}

		s9xcommand_t	*c = (s9xcommand_t *) calloc(j, sizeof(s9xcommand_t));
		if (c == NULL)
		{
			perror("malloc error while parsing multi");
			return (cmd);
		}

		n = 0;
		i = (name[0] == '+') ? 2 : 1;

		do
		{
			if (name[i] == ';')
			{
				c[n].type         = S9xNoMapping;
				c[n].multi_press  = 0;
				c[n].button_norpt = 0;

				j = i;
			}
			else if (name[i] == ',')
			{
				free(c);
				return (cmd);
			}
			else
			{
				uint8	press = 0;

				if (name[0] == '+')
				{
					if (name[i] == '+')
						press = 1;
					else if (name[i] == '-')
						press = 2;
					else
					{
						free(c);
						return (cmd);
					}

					i++;
				}

				for (j = i; name[j] != ';' && name[j] != ',' && name[j] != '}'; j++) ;

				x.assign(name + i, j - i);
				c[n] = S9xGetCommandT(x.c_str());
				c[n].multi_press = press;

				if (maptype(c[n].type) != MAP_BUTTON)
				{
					free(c);
					return (cmd);
				}

				if (name[j] == ';')
					j--;
			}

			i = j + 1;
			n++;
		}
		while (name[i] != '\0');

		c[n].type        = S9xNoMapping;
		c[n].multi_press = 3;

		multis.push_back(c);

		cmd.button.multi_idx = multis.size() - 1;
		cmd.type = S9xButtonMulti;
	}
	else
	{
		i = findstr(name, command_names, LAST_COMMAND);
		if (i < 0)
			return (cmd);

		cmd.type = S9xButtonCommand;
		cmd.button.command = i;
	}

	return (cmd);
}

s9xcommand_t S9xGetMapping (uint32 id)
{
	if (keymap.count(id) == 0)
	{
		s9xcommand_t	cmd;
		cmd.type = S9xNoMapping;
		return (cmd);
	}
	else
		return (keymap[id]);
}

static const char * maptypename (int t)
{
	switch (t)
	{
		case MAP_NONE:		return ("unmapped");
		case MAP_BUTTON:	return ("button");
		case MAP_AXIS:		return ("axis");
		case MAP_POINTER:	return ("pointer");
		default:			return ("unknown");
	}
}

void S9xUnmapID (uint32 id)
{
	for (int i = 0; i < NUMCTLS + 1; i++)
		pollmap[i].erase(id);

	if (id >= PseudoPointerBase)
		pseudopointer[id - PseudoPointerBase].mapped = false;

	keymap.erase(id);
}

static int32 ApplyMulti (s9xcommand_t *multi, int32 pos, int16 data1)
{
	while (1)
	{
		if (multi[pos].multi_press == 3)
			return (-1);

		if (multi[pos].type == S9xNoMapping)
			break;

		if (multi[pos].multi_press)
			S9xApplyCommand(multi[pos], multi[pos].multi_press == 1, 0);
		else
			S9xApplyCommand(multi[pos], data1, 0);

		pos++;
	}

	return (pos + 1);
}

void S9xApplyCommand (s9xcommand_t cmd, int16 data1, int16 data2)
{
	int	i;

	switch (cmd.type)
	{
		case S9xNoMapping:
			return;

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
					#ifdef DEBUGGER
						CPU.Flags |= DEBUG_MODE_FLAG;
					#endif
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
					#if defined(NETPLAY_SUPPORT) && !defined(__WIN32__)
						S9xNPSendPause(Settings.Paused);
					#endif
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

#ifdef NETPLAY_SUPPORT
						if (Settings.NetPlay && data2 != 1) { //data2 == 1 means it's sent by the netplay code
							if (Settings.NetPlayServer) {
								S9xNPSendJoypadSwap();
							} else {
								S9xSetInfoString("Netplay Client cannot swap pads.");
								break;
							}
						}
#endif

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

		case S9xPointer:
			return;

		case S9xButtonPseudopointer:
			if (data1)
			{
				if (cmd.button.pointer.UD)
				{
					if (!pseudopointer[cmd.button.pointer.idx].V_adj)
						pseudopointer[cmd.button.pointer.idx].V_adj = cmd.button.pointer.UD * ptrspeeds[cmd.button.pointer.speed_type];
					pseudopointer[cmd.button.pointer.idx].V_var = (cmd.button.pointer.speed_type == 0);
				}

				if (cmd.button.pointer.LR)
				{
					if (!pseudopointer[cmd.button.pointer.idx].H_adj)
						pseudopointer[cmd.button.pointer.idx].H_adj = cmd.button.pointer.LR * ptrspeeds[cmd.button.pointer.speed_type];
					pseudopointer[cmd.button.pointer.idx].H_var = (cmd.button.pointer.speed_type == 0);
				}
			}
			else
			{
				if (cmd.button.pointer.UD)
				{
					pseudopointer[cmd.button.pointer.idx].V_adj = 0;
					pseudopointer[cmd.button.pointer.idx].V_var = false;
				}

				if (cmd.button.pointer.LR)
				{
					pseudopointer[cmd.button.pointer.idx].H_adj = 0;
					pseudopointer[cmd.button.pointer.idx].H_var = false;
				}
			}

			return;

		case S9xAxisJoypad:
		{
			uint16	pos, neg;

			switch (cmd.axis.joypad.axis)
			{
				case 0: neg = SNES_LEFT_MASK;	pos = SNES_RIGHT_MASK;	break;
				case 1: neg = SNES_UP_MASK;		pos = SNES_DOWN_MASK;	break;
				case 2: neg = SNES_Y_MASK;		pos = SNES_A_MASK;		break;
				case 3: neg = SNES_X_MASK;		pos = SNES_B_MASK;		break;
				case 4: neg = SNES_TL_MASK;		pos = SNES_TR_MASK;		break;
				default: return;
			}

			if (cmd.axis.joypad.invert)
				data1 = -data1;

			uint16	p, r;

			p = r = 0;
			if (data1 >  ((cmd.axis.joypad.threshold + 1) *  127))
				p |= pos;
			else
				r |= pos;

			if (data1 <= ((cmd.axis.joypad.threshold + 1) * -127))
				p |= neg;
			else
				r |= neg;

			joypad[cmd.axis.joypad.idx].buttons |= p;
			joypad[cmd.axis.joypad.idx].buttons &= ~r;
			joypad[cmd.axis.joypad.idx].turbos  &= ~(p | r);

			return;
		}

		case S9xAxisPseudopointer:
			if (data1 == 0)
			{
				if (cmd.axis.pointer.HV)
				{
					pseudopointer[cmd.axis.pointer.idx].V_adj = 0;
					pseudopointer[cmd.axis.pointer.idx].V_var = false;
				}
				else
				{
					pseudopointer[cmd.axis.pointer.idx].H_adj = 0;
					pseudopointer[cmd.axis.pointer.idx].H_var = false;
				}
			}
			else
			{
				if (cmd.axis.pointer.invert)
					data1 = -data1;

				if (cmd.axis.pointer.HV)
				{
					if (!pseudopointer[cmd.axis.pointer.idx].V_adj)
						pseudopointer[cmd.axis.pointer.idx].V_adj = (int16) ((int32) data1 * ptrspeeds[cmd.axis.pointer.speed_type] / 32767);
					pseudopointer[cmd.axis.pointer.idx].V_var = (cmd.axis.pointer.speed_type == 0);
				}
				else
				{
					if (!pseudopointer[cmd.axis.pointer.idx].H_adj)
						pseudopointer[cmd.axis.pointer.idx].H_adj = (int16) ((int32) data1 * ptrspeeds[cmd.axis.pointer.speed_type] / 32767);
					pseudopointer[cmd.axis.pointer.idx].H_var = (cmd.axis.pointer.speed_type == 0);
				}
			}

			return;

		case S9xAxisPseudobuttons:
			if (data1 >  ((cmd.axis.button.threshold + 1) *  127))
			{
				if (!pseudobuttons[cmd.axis.button.posbutton])
				{
					pseudobuttons[cmd.axis.button.posbutton] = 1;
					S9xReportButton(PseudoButtonBase + cmd.axis.button.posbutton, true);
				}
			}
			else
			{
				if (pseudobuttons[cmd.axis.button.posbutton])
				{
					pseudobuttons[cmd.axis.button.posbutton] = 0;
					S9xReportButton(PseudoButtonBase + cmd.axis.button.posbutton, false);
				}
			}

			if (data1 <= ((cmd.axis.button.threshold + 1) * -127))
			{
				if (!pseudobuttons[cmd.axis.button.negbutton])
				{
					pseudobuttons[cmd.axis.button.negbutton] = 1;
					S9xReportButton(PseudoButtonBase + cmd.axis.button.negbutton, true);
				}
			}
			else
			{
				if (pseudobuttons[cmd.axis.button.negbutton])
				{
					pseudobuttons[cmd.axis.button.negbutton] = 0;
					S9xReportButton(PseudoButtonBase + cmd.axis.button.negbutton, false);
				}
			}

			return;

		case S9xButtonPort:
		case S9xAxisPort:
		case S9xPointerPort:
			S9xHandlePortCommand(cmd, data1, data2);
			return;

		case S9xButtonMulti:
			if (cmd.button.multi_idx >= (int) multis.size())
				return;

			if (multis[cmd.button.multi_idx]->multi_press && !data1)
				return;

			i = ApplyMulti(multis[cmd.button.multi_idx], 0, data1);
			if (i >= 0)
			{
				struct exemulti	*e = new struct exemulti;
				e->pos    = i;
				e->data1  = data1 != 0;
				e->script = multis[cmd.button.multi_idx];
				exemultis.insert(e);
			}

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
	int					i, j;

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

	for (int n = 0; n < 8; n++)
	{
		if (!pseudopointer[n].mapped)
			continue;

		if (pseudopointer[n].H_adj)
		{
			pseudopointer[n].x += pseudopointer[n].H_adj;
			if (pseudopointer[n].x < 0)
				pseudopointer[n].x = 0;
			else if (pseudopointer[n].x > 255)
				pseudopointer[n].x = 255;

			if (pseudopointer[n].H_var)
			{
				if (pseudopointer[n].H_adj < 0)
				{
					if (pseudopointer[n].H_adj > -ptrspeeds[3])
						pseudopointer[n].H_adj--;
				}
				else
				{
					if (pseudopointer[n].H_adj <  ptrspeeds[3])
						pseudopointer[n].H_adj++;
				}
			}
		}

		if (pseudopointer[n].V_adj)
		{
			pseudopointer[n].y += pseudopointer[n].V_adj;
			if (pseudopointer[n].y < 0)
				pseudopointer[n].y = 0;
			else if (pseudopointer[n].y > PPU.ScreenHeight - 1)
				pseudopointer[n].y = PPU.ScreenHeight - 1;

			if (pseudopointer[n].V_var)
			{
				if (pseudopointer[n].V_adj < 0)
				{
					if (pseudopointer[n].V_adj > -ptrspeeds[3])
						pseudopointer[n].V_adj--;
				}
				else
				{
					if (pseudopointer[n].V_adj <  ptrspeeds[3])
						pseudopointer[n].V_adj++;
				}
			}
		}

		// S9xReportPointer removed (old mapping system). pseudopointer[n].mapped is always false anyway.
	}

	set<struct exemulti *>::iterator	it, jt;

	for (it = exemultis.begin(); it != exemultis.end(); it++)
	{
		i = ApplyMulti((*it)->script, (*it)->pos, (*it)->data1);

		if (i >= 0)
			(*it)->pos = i;
		else
		{
			jt = it;
			it--;
			delete *jt;
			exemultis.erase(jt);
		}
	}

	do_polling(POLL_ALL);

	pad_read_last = pad_read;
	pad_read      = false;
}

void S9xControlPreSaveState (struct SControlSnapshot *s)
{
	memset(s, 0, sizeof(*s));
	s->ver = 4;

	for (int j = 0; j < 2; j++)
	{
		s->port1_read_idx[j] = read_idx[0][j];
		s->port2_read_idx[j] = read_idx[1][j];
	}

#define COPY(x)	{ memcpy((char *) s->internal + i, &(x), sizeof(x)); i += sizeof(x); }
#define SKIP(n)	{ i += (n); }

	int	i = 0;

	for (int j = 0; j < 8; j++)
		COPY(joypad[j].buttons);

	// Skip mouse[2] data (delta_x, delta_y, old_x, old_y, cur_x, cur_y, buttons) * 2
	SKIP(2 * (1 + 1 + 2 + 2 + 2 + 2 + 1));

	// Skip superscope data (x, y, phys_buttons, next_buttons, read_buttons)
	SKIP(2 + 2 + 1 + 1 + 1);

	// Skip justifier data (x[2], y[2], buttons, offscreen[2])
	SKIP(2 + 2 + 2 + 2 + 1 + 1 + 1);

	for (int j = 0; j < 2; j++)
		for (int k = 0; k < 2; k++)
			COPY(mp5[j].pads[k]);

	assert(i == sizeof(s->internal));

#undef COPY
#undef SKIP

	s->pad_read      = pad_read;
	s->pad_read_last = pad_read_last;
}

void S9xControlPostLoadState (struct SControlSnapshot *s)
{
	if (curcontrollers[0] == MP5 && s->ver < 1)
	{
		// Crap. Old snes9x didn't support this.
		S9xMessage(S9X_WARNING, S9X_FREEZE_FILE_INFO, "Old savestate has no support for MP5 in port 1.");
		newcontrollers[0] = curcontrollers[0];
		curcontrollers[0] = mp5[0].pads[0];
	}

	for (int j = 0; j < 2; j++)
	{
		read_idx[0][j] = s->port1_read_idx[j];
		read_idx[1][j] = s->port2_read_idx[j];
	}

	FLAG_LATCH = (Memory.FillRAM[0x4016] & 1) == 1;

	if (s->ver > 1)
	{
	#define COPY(x)	{ memcpy(&(x), (char *) s->internal + i, sizeof(x)); i += sizeof(x); }
	#define SKIP(n)	{ i += (n); }

		int	i = 0;

		for (int j = 0; j < 8; j++)
			COPY(joypad[j].buttons);

		// Skip mouse[2] data
		SKIP(2 * (1 + 1 + 2 + 2 + 2 + 2 + 1));

		// Skip superscope data
		SKIP(2 + 2 + 1 + 1 + 1);

		// Skip justifier data
		SKIP(2 + 2 + 2 + 2 + 1 + 1 + 1);

		for (int j = 0; j < 2; j++)
			for (int k = 0; k < 2; k++)
				COPY(mp5[j].pads[k]);

		assert(i == sizeof(s->internal));

	#undef COPY
	#undef SKIP
	}

	if (s->ver > 2)
	{
		pad_read      = s->pad_read;
		pad_read_last = s->pad_read_last;
	}
}

uint16 MovieGetJoypad (int i)
{
	if (i < 0 || i > 7)
		return (0);

	return (joypad[i].buttons);
}

void MovieSetJoypad (int i, uint16 buttons)
{
	if (i < 0 || i > 7)
		return;

	joypad[i].buttons = buttons;
}

void S9xSetJoypadButtons (int pad, uint16 buttons)
{
	if (pad < 0 || pad > 7)
		return;

	joypad[pad].buttons = buttons;
}

