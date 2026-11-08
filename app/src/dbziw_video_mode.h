#pragma once
// DBZIW 019: which video mode a run starts in, and why (dbziw_video_mode.cpp; CTest video_mode_choice).
#include <string>

namespace dbziw::video
{
    struct Choice
    {
        unsigned hz = 50u;
        const char *source = "the disc's default";
        bool invalid = false; // a value other than 60 or 50 was given (the game then starts at 50 Hz)
    };

    // First match wins: PS2X_VIDEO_MODE (test runs), the player's video_mode line in dbziw.ini, the launcher's default
    // (PS2X_VIDEO_MODE_DEFAULT, set when the launcher starts the game: 60), else the PAL disc's own 50 Hz.
    inline Choice choose(const char *env, const std::string *ini, const char *launcherDefault)
    {
        Choice choice;
        std::string value;
        if (env && *env)
        {
            value = env;
            choice.source = "PS2X_VIDEO_MODE";
        }
        else if (ini)
        {
            value = *ini;
            choice.source = "dbziw.ini";
        }
        else if (launcherDefault && *launcherDefault)
        {
            value = launcherDefault;
            choice.source = "the launcher's default";
        }
        else
            return choice;
        if (value == "60")
            choice.hz = 60u;
        else if (value != "50")
            choice.invalid = true;
        return choice;
    }
}
