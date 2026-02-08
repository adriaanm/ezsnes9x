/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "config.h"
#include "controls.h"
#include <fstream>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Minimal YAML-subset parser helpers
// ---------------------------------------------------------------------------

static std::string trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool parse_bool(const std::string &value, bool &out)
{
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    if (v == "true" || v == "yes" || v == "on" || v == "1")
    {
        out = true;
        return true;
    }
    if (v == "false" || v == "no" || v == "off" || v == "0")
    {
        out = false;
        return true;
    }
    return false;
}

static bool parse_int(const std::string &value, int &out)
{
    if (value.empty())
        return false;

    char *end = nullptr;
    long v = strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0')
        return false;

    out = (int)v;
    return true;
}

static bool file_exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// ---------------------------------------------------------------------------
// Map a controller string ("pad1", "none", "mp5", etc.) to a controller enum
// and id values suitable for S9xSetController.
// ---------------------------------------------------------------------------

static void parse_controller(int port, const std::string &value)
{
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    if (v == "none" || v == "")
    {
        S9xSetController(port, CTL_NONE, -1, -1, -1, -1);
    }
    else if (v == "mp5" || v == "multitap")
    {
        // Multitap on this port: assign pads starting from port*4
        int base = port * 4;
        S9xSetController(port, CTL_MP5, base, base + 1, base + 2, base + 3);
    }
    else if (v == "mouse" || v == "mouse0")
    {
        S9xSetController(port, CTL_MOUSE, 0, -1, -1, -1);
    }
    else if (v == "mouse1")
    {
        S9xSetController(port, CTL_MOUSE, 1, -1, -1, -1);
    }
    else if (v == "superscope" || v == "scope")
    {
        S9xSetController(port, CTL_SUPERSCOPE, 0, -1, -1, -1);
    }
    else if (v == "justifier")
    {
        S9xSetController(port, CTL_JUSTIFIER, 0, -1, -1, -1);
    }
    else if (v == "justifier2" || v == "justifiers")
    {
        S9xSetController(port, CTL_JUSTIFIER, 1, -1, -1, -1);
    }
    else if (v == "macsrifle")
    {
        S9xSetController(port, CTL_MACSRIFLE, 0, -1, -1, -1);
    }
    else
    {
        // Default: treat as joypad. "pad0" through "pad7", or just "pad1", etc.
        int id = 0;
        if (v.size() >= 4 && v.substr(0, 3) == "pad")
        {
            int n = 0;
            if (parse_int(v.substr(3), n) && n >= 0 && n <= 7)
                id = n;
        }
        S9xSetController(port, CTL_JOYPAD, id, -1, -1, -1);
    }
}

// ---------------------------------------------------------------------------
// Apply a parsed key-value pair to Settings / S9xConfig
// ---------------------------------------------------------------------------

static void apply_setting(const std::string &section,
                          const std::string &key,
                          const std::string &value,
                          S9xConfig &config)
{
    bool bval;
    int  ival;

    if (section.empty())
    {
        // Top-level keys
        if (key == "rom")
            config.rom_path = value;
        else if (key == "save_dir")
            config.save_dir = value;
    }
    else if (section == "audio")
    {
        if (key == "sample_rate" && parse_int(value, ival))
            Settings.SoundPlaybackRate = (uint32)ival;
        else if (key == "stereo" && parse_bool(value, bval))
            Settings.Stereo = bval ? TRUE : FALSE;
        else if (key == "mute" && parse_bool(value, bval))
            Settings.Mute = bval ? TRUE : FALSE;
    }
    else if (section == "video")
    {
        if (key == "transparency" && parse_bool(value, bval))
            Settings.Transparency = bval ? TRUE : FALSE;
    }
    else if (section == "input")
    {
        if (key == "multitap" && parse_bool(value, bval))
            Settings.MultiPlayer5Master = bval ? TRUE : FALSE;
        else if (key == "up_and_down" && parse_bool(value, bval))
            Settings.UpAndDown = bval ? TRUE : FALSE;
        else if (key == "player1")
            parse_controller(0, value);
        else if (key == "player2")
            parse_controller(1, value);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool S9xLoadConfig(const char *filename, S9xConfig &config)
{
    std::ifstream file(filename);
    if (!file.is_open())
        return false;

    std::string line;
    std::string current_section;

    while (std::getline(file, line))
    {
        // Strip trailing \r for Windows line endings
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip blank lines
        if (trim(line).empty())
            continue;

        // Skip comment lines
        std::string trimmed = trim(line);
        if (trimmed[0] == '#')
            continue;

        // Strip inline comments
        size_t comment_pos = trimmed.find(" #");
        if (comment_pos != std::string::npos)
            trimmed = trim(trimmed.substr(0, comment_pos));

        // Determine indentation level
        size_t indent = 0;
        while (indent < line.size() && line[indent] == ' ')
            indent++;

        // Find the colon separator
        size_t colon = trimmed.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key   = trim(trimmed.substr(0, colon));
        std::string value = trim(trimmed.substr(colon + 1));

        if (indent == 0)
        {
            if (value.empty())
            {
                // This is a section header (e.g. "audio:")
                current_section = key;
            }
            else
            {
                // Top-level key: value
                current_section.clear();
                apply_setting("", key, value, config);
            }
        }
        else
        {
            // Nested key under current_section
            apply_setting(current_section, key, value, config);
        }
    }

    return true;
}

std::string S9xGetDefaultConfigPath()
{
    // 1. Check current directory
    if (file_exists("snes9x.yml"))
        return "snes9x.yml";

    // 2. Check $HOME/.snes9x/snes9x.yml
    const char *home = getenv("HOME");
    if (home)
    {
        std::string path = std::string(home) + "/.snes9x/snes9x.yml";
        if (file_exists(path))
            return path;
    }

    // 3. Check XDG_CONFIG_HOME or $HOME/.config/snes9x/snes9x.yml
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg)
    {
        std::string path = std::string(xdg) + "/snes9x/snes9x.yml";
        if (file_exists(path))
            return path;
    }
    else if (home)
    {
        std::string path = std::string(home) + "/.config/snes9x/snes9x.yml";
        if (file_exists(path))
            return path;
    }

#ifdef _WIN32
    // 4. Check %APPDATA%\snes9x\snes9x.yml on Windows
    const char *appdata = getenv("APPDATA");
    if (appdata)
    {
        std::string path = std::string(appdata) + "\\snes9x\\snes9x.yml";
        if (file_exists(path))
            return path;
    }
#endif

    // Return empty string if no config file found
    return "";
}
