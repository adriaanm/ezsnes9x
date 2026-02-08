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
// Apply a parsed key-value pair to Settings / S9xConfig
// ---------------------------------------------------------------------------

static void apply_setting(const std::string &section,
                          const std::string &key,
                          const std::string &value,
                          S9xConfig &config)
{
    bool bval;
    int ival;

    if (section.empty())
    {
        // Top-level keys
        if (key == "save_dir")
            config.save_dir = value;
        else if (key == "rewind_enabled" && parse_bool(value, bval))
            config.rewind_enabled = bval;
    }
    else if (section == "keyboard")
    {
        if (key == "port" && parse_int(value, ival))
            config.keyboard.port = ival;
        else if (parse_int(value, ival))
            // Store keycode for this button mapping
            config.keyboard.button_to_keycode[key] = ival;
    }
    else if (section == "controller")
    {
        // Start a new controller mapping
        if (key == "matching")
        {
            S9xControllerMapping mapping;
            mapping.matching = value;
            config.controllers.push_back(mapping);
        }
        else if (key == "port" && parse_int(value, ival) && !config.controllers.empty())
        {
            config.controllers.back().port = ival;
        }
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
