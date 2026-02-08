/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include <ctype.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "snes9x.h"
#include "memmap.h"
#include "controls.h"
#include "display.h"
#define S9X_CONF_FILE_NAME	"snes9x.conf"

static char	*rom_filename = NULL;

static bool parse_controller_spec (int, const char *);


static bool parse_controller_spec (int port, const char *arg)
{
	if (!strcasecmp(arg, "none"))
		S9xSetController(port, CTL_NONE,       0, 0, 0, 0);
	else if (!strncasecmp(arg, "pad",   3) && arg[3] >= '1' && arg[3] <= '8' && arg[4] == '\0')
		S9xSetController(port, CTL_JOYPAD, arg[3] - '1', 0, 0, 0);
	else if (!strncasecmp(arg, "mp5:", 4) && ((arg[4] >= '1' && arg[4] <= '8') || arg[4] == 'n') &&
										((arg[5] >= '1' && arg[5] <= '8') || arg[5] == 'n') &&
										((arg[6] >= '1' && arg[6] <= '8') || arg[6] == 'n') &&
										((arg[7] >= '1' && arg[7] <= '8') || arg[7] == 'n') && arg[8] == '\0')
		S9xSetController(port, CTL_MP5, (arg[4] == 'n') ? -1 : arg[4] - '1',
										(arg[5] == 'n') ? -1 : arg[5] - '1',
										(arg[6] == 'n') ? -1 : arg[6] - '1',
										(arg[7] == 'n') ? -1 : arg[7] - '1');
	else
		return (false);

	return (true);
}

void S9xLoadConfigFiles (char **argv, int argc)
{
	S9xVerifyControllers();
}

void S9xUsage (void)
{
	/*                               12345678901234567890123456789012345678901234567890123456789012345678901234567890 */

	S9xMessage(S9X_INFO, S9X_USAGE, "");
	S9xMessage(S9X_INFO, S9X_USAGE, "Snes9x " VERSION);
	S9xMessage(S9X_INFO, S9X_USAGE, "");
	S9xMessage(S9X_INFO, S9X_USAGE, "usage: snes9x [options] <ROM image filename>");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	// SOUND OPTIONS
	S9xMessage(S9X_INFO, S9X_USAGE, "-soundsync                      Synchronize sound as far as possible");
	S9xMessage(S9X_INFO, S9X_USAGE, "-playbackrate <Hz>              Set sound playback rate");
	S9xMessage(S9X_INFO, S9X_USAGE, "-inputrate <Hz>                 Set sound input rate");
	S9xMessage(S9X_INFO, S9X_USAGE, "-reversestereo                  Reverse stereo sound output");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nostereo                       Disable stereo sound output");
	S9xMessage(S9X_INFO, S9X_USAGE, "-eightbit                       Use 8bit sound instead of 16bit");
	S9xMessage(S9X_INFO, S9X_USAGE, "-mute                           Mute sound");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	// DISPLAY OPTIONS
	S9xMessage(S9X_INFO, S9X_USAGE, "-displaytime                    Display the time");
	S9xMessage(S9X_INFO, S9X_USAGE, "-displayframerate               Display the frame rate counter");
	S9xMessage(S9X_INFO, S9X_USAGE, "-displaykeypress                Display input of all controllers and peripherals");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nohires                        (Not recommended) Disable support for hi-res and");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                interlace modes");
	S9xMessage(S9X_INFO, S9X_USAGE, "-notransparency                 (Not recommended) Disable transparency effects");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nowindows                      (Not recommended) Disable graphic window effects");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	// CONTROLLER OPTIONS
	S9xMessage(S9X_INFO, S9X_USAGE, "-nomp5                          Disable emulation of the Multiplayer 5 adapter");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nomouse                        Disable emulation of the SNES mouse");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nosuperscope                   Disable emulation of the Superscope");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nojustifier                    Disable emulation of the Konami Justifier");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nomacsrifle                    Disable emulation of the M.A.C.S. Rifle");
	S9xMessage(S9X_INFO, S9X_USAGE, "-port# <control>                Specify which controller to emulate in port 1/2");
	S9xMessage(S9X_INFO, S9X_USAGE, "    Controllers: none              No controller");
	S9xMessage(S9X_INFO, S9X_USAGE, "                 pad#              Joypad number 1-8");
	S9xMessage(S9X_INFO, S9X_USAGE, "                 mouse#            Mouse number 1-2");
	S9xMessage(S9X_INFO, S9X_USAGE, "                 superscope        Superscope (not useful with -port1)");
	S9xMessage(S9X_INFO, S9X_USAGE, "                 justifier         Blue Justifier (not useful with -port1)");
	S9xMessage(S9X_INFO, S9X_USAGE, "                 two-justifiers    Blue & Pink Justifiers");
	S9xMessage(S9X_INFO, S9X_USAGE, "                 mp5:####          MP5 with the 4 named pads (1-8 or n)");
	S9xMessage(S9X_INFO, S9X_USAGE, "                 macsrifle         M.A.C.S. Rifle");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	// ROM OPTIONS
	S9xMessage(S9X_INFO, S9X_USAGE, "-hirom                          Force Hi-ROM memory map");
	S9xMessage(S9X_INFO, S9X_USAGE, "-lorom                          Force Lo-ROM memory map");
	S9xMessage(S9X_INFO, S9X_USAGE, "-ntsc                           Force NTSC timing (60 frames/sec)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-pal                            Force PAL timing (50 frames/sec)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nointerleave                   Assume the ROM image is not in interleaved");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                format");
	S9xMessage(S9X_INFO, S9X_USAGE, "-interleaved                    Assume the ROM image is in interleaved format");
	S9xMessage(S9X_INFO, S9X_USAGE, "-interleaved2                   Assume the ROM image is in interleaved 2 format");
	S9xMessage(S9X_INFO, S9X_USAGE, "-interleavedgd24                Assume the ROM image is in interleaved gd24");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                format");
	S9xMessage(S9X_INFO, S9X_USAGE, "-noheader                       Assume the ROM image doesn't have a header of a");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                copier");
	S9xMessage(S9X_INFO, S9X_USAGE, "-header                         Assume the ROM image has a header of a copier");
	S9xMessage(S9X_INFO, S9X_USAGE, "-bsxbootup                      Boot up BS games from BS-X");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	// PATCH/CHEAT OPTIONS
	S9xMessage(S9X_INFO, S9X_USAGE, "-nopatch                        Do not apply any available IPS/UPS patches");
	S9xMessage(S9X_INFO, S9X_USAGE, "-cheat                          Apply saved cheats");
	S9xMessage(S9X_INFO, S9X_USAGE, "-cheatcode <code>               Supply a cheat code in Game Genie,");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                Pro-Action Replay, or Raw format (address=byte)");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

#ifdef NETPLAY_SUPPORT
	// NETPLAY OPTIONS
	S9xMessage(S9X_INFO, S9X_USAGE, "-net                            Enable netplay");
	S9xMessage(S9X_INFO, S9X_USAGE, "-port <num>                     Use port <num> for netplay (use with -net)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-server <string>                Use the specified server for netplay");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                (use with -net)");
	S9xMessage(S9X_INFO, S9X_USAGE, "");
#endif

	// HACKING OR DEBUGGING OPTIONS
#ifdef DEBUGGER
	S9xMessage(S9X_INFO, S9X_USAGE, "-debug                          Set the Debugger flag");
	S9xMessage(S9X_INFO, S9X_USAGE, "-trace                          Begin CPU instruction tracing");
#endif
	S9xMessage(S9X_INFO, S9X_USAGE, "-hdmatiming <1-199>             (Not recommended) Changes HDMA transfer timings");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                event comes");
	S9xMessage(S9X_INFO, S9X_USAGE, "-invalidvramaccess              (Not recommended) Allow invalid VRAM access");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	// OTHER OPTIONS
	S9xMessage(S9X_INFO, S9X_USAGE, "-frameskip <num>                Screen update frame skip rate");
	S9xMessage(S9X_INFO, S9X_USAGE, "-frametime <num>                Milliseconds per frame for frameskip auto-adjust");
	S9xMessage(S9X_INFO, S9X_USAGE, "-upanddown                      Override protection from pressing left+right or");
	S9xMessage(S9X_INFO, S9X_USAGE, "                                up+down together");
	S9xMessage(S9X_INFO, S9X_USAGE, "-conf <filename>                Use specified conf file (after standard files)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-nostdconf                      Do not load the standard config files");
	S9xMessage(S9X_INFO, S9X_USAGE, "");

	S9xExtraUsage();

	S9xMessage(S9X_INFO, S9X_USAGE, "");
	S9xMessage(S9X_INFO, S9X_USAGE, "ROM image can be compressed with zip, gzip, JMA, or compress.");

	exit(1);
}

char * S9xParseArgs (char **argv, int argc)
{
	for (int i = 1; i < argc; i++)
	{
		if (*argv[i] == '-')
		{
			if (!strcasecmp(argv[i], "-help"))
				S9xUsage();
			else

			// SOUND OPTIONS

			if (!strcasecmp(argv[i], "-soundsync"))
				Settings.SoundSync = TRUE;
			else if (!strcasecmp(argv[i], "-dynamicratecontrol"))
			{
				Settings.DynamicRateControl = TRUE;
				Settings.DynamicRateLimit = 5;
			}
			else
			if (!strcasecmp(argv[i], "-playbackrate"))
			{
				if (i + 1 < argc)
				{
					Settings.SoundPlaybackRate = atoi(argv[++i]);
					if (Settings.SoundPlaybackRate < 8192)
						Settings.SoundPlaybackRate = 8192;
				}
				else
					S9xUsage();
			}
			else
			if (!strcasecmp(argv[i], "-inputrate"))
			{
				if (i + 1 < argc)
				{
					Settings.SoundInputRate = atoi(argv[++i]);
					if (Settings.SoundInputRate < 31700)
						Settings.SoundInputRate = 31700;
					if (Settings.SoundInputRate > 32300)
						Settings.SoundInputRate = 32300;
				}
				else
					S9xUsage();
			}
			else
			if (!strcasecmp(argv[i], "-reversestereo"))
				Settings.ReverseStereo = TRUE;
			else
			if (!strcasecmp(argv[i], "-nostereo"))
				Settings.Stereo = FALSE;
			else
			if (!strcasecmp(argv[i], "-eightbit"))
				Settings.SixteenBitSound = FALSE;
			else
			if (!strcasecmp(argv[i], "-mute"))
				Settings.Mute = TRUE;
			else

			// DISPLAY OPTIONS

			if (!strcasecmp(argv[i], "-notransparency"))
				Settings.Transparency = FALSE;
			else
			if (!strcasecmp(argv[i], "-nowindows"))
				Settings.DisableGraphicWindows = TRUE;
			else

			// CONTROLLER OPTIONS

			if (!strcasecmp(argv[i], "-nomp5"))
				Settings.MultiPlayer5Master = FALSE;
			else
			if (!strcasecmp(argv[i], "-port1") ||
				!strcasecmp(argv[i], "-port2"))
			{
				if (i + 1 < argc)
				{
					i++;
					if (!parse_controller_spec(argv[i - 1][5] - '1', argv[i]))
						S9xUsage();
				}
				else
					S9xUsage();
			}
			else

			// ROM OPTIONS

			if (!strcasecmp(argv[i], "-hirom"))
				Settings.ForceHiROM = TRUE;
			else
			if (!strcasecmp(argv[i], "-lorom"))
				Settings.ForceLoROM = TRUE;
			else
			if (!strcasecmp(argv[i], "-ntsc"))
				Settings.ForceNTSC = TRUE;
			else
			if (!strcasecmp(argv[i], "-pal"))
				Settings.ForcePAL = TRUE;
			else
			if (!strcasecmp(argv[i], "-nointerleave"))
				Settings.ForceNotInterleaved = TRUE;
			else
			if (!strcasecmp(argv[i], "-interleaved"))
				Settings.ForceInterleaved = TRUE;
			else
			if (!strcasecmp(argv[i], "-interleaved2"))
				Settings.ForceInterleaved2 = TRUE;
			else
			if (!strcasecmp(argv[i], "-interleavedgd24"))
				Settings.ForceInterleaveGD24 = TRUE;
			else
			if (!strcasecmp(argv[i], "-noheader"))
				Settings.ForceNoHeader = TRUE;
			else
			if (!strcasecmp(argv[i], "-header"))
				Settings.ForceHeader = TRUE;
			else
			if (!strcasecmp(argv[i], "-bsxbootup"))
				Settings.BSXBootup = TRUE;
                        else
                        if (!strcasecmp(argv[i], "-snapshot"))
                        {
                                if (i + 1 < argc)
                                {
                                        strncpy(Settings.InitialSnapshotFilename, argv[++i], PATH_MAX);
                                        Settings.InitialSnapshotFilename[PATH_MAX] = 0;
                                }
                                else
                                        S9xUsage();
                        }
			else

			// PATCH/CHEAT OPTIONS

			if (!strcasecmp(argv[i], "-nopatch"))
				Settings.NoPatch = TRUE;
			else
			// NETPLAY OPTIONS

		#ifdef NETPLAY_SUPPORT
			if (!strcasecmp(argv[i], "-net"))
				Settings.NetPlay = TRUE;
			else
			if (!strcasecmp(argv[i], "-port"))
			{
				if (i + 1 < argc)
					Settings.Port = -atoi(argv[++i]);
				else
					S9xUsage();
			}
			else
			if (!strcasecmp(argv[i], "-server"))
			{
				if (i + 1 < argc)
				{
					strncpy(Settings.ServerName, argv[++i], 127);
					Settings.ServerName[127] = 0;
				}
				else
					S9xUsage();
			}
			else
		#endif

			// HACKING OR DEBUGGING OPTIONS

		#ifdef DEBUGGER
			if (!strcasecmp(argv[i], "-debug"))
				CPU.Flags |= DEBUG_MODE_FLAG;
			else
			if (!strcasecmp(argv[i], "-trace"))
			{
				ENSURE_TRACE_OPEN(trace,"trace.log","wb")
				CPU.Flags |= TRACE_FLAG;
			}
			else
		#endif

			if (!strcasecmp(argv[i], "-hdmatiming"))
			{
				if (i + 1 < argc)
				{
					int	p = atoi(argv[++i]);
					if (p > 0 && p < 200)
						Settings.HDMATimingHack = p;
				}
				else
					S9xUsage();
			}
			else
			if (!strcasecmp(argv[i], "-invalidvramaccess"))
				Settings.BlockInvalidVRAMAccessMaster = FALSE;
			else

			// OTHER OPTIONS

			if (!strcasecmp(argv[i], "-frameskip"))
			{
				if (i + 1 < argc)
					Settings.SkipFrames = atoi(argv[++i]) + 1;
				else
					S9xUsage();
			}
			else
			if (!strcasecmp(argv[i], "-frametime"))
			{
				if (i + 1 < argc)
					Settings.FrameTimePAL = Settings.FrameTimeNTSC = atoi(argv[++i]);
				else
					S9xUsage();
			}
			else
			if (!strcasecmp(argv[i], "-conf"))
			{
				if (++i >= argc)
					S9xUsage();
				// Else do nothing, S9xLoadConfigFiles() handled it.
			}
			else
			if (!strcasecmp(argv[i], "-nostdconf"))
			{
				// Do nothing, S9xLoadConfigFiles() handled it.
			}
			else
				S9xParseArg(argv, i, argc);
		}
		else
			rom_filename = argv[i];
	}

	S9xVerifyControllers();

	return (rom_filename);
}
