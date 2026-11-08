// DBZIW 019: which video mode a run starts in. First match wins: PS2X_VIDEO_MODE (tests), the player's video_mode line,
// the launcher's default (60), else the PAL disc's 50 Hz; a value other than 60 or 50 starts at 50 Hz and says so.
#include "dbziw_video_mode.h"

#include <cstring>
#include <iostream>
#include <string>

namespace
{
    int failures = 0;

    void check(const dbziw::video::Choice &c, unsigned hz, const char *source, bool invalid, const char *what)
    {
        const bool ok = c.hz == hz && std::strcmp(c.source, source) == 0 && c.invalid == invalid;
        std::cout << (ok ? "PASS " : "FAIL ") << what << " (" << c.hz << " Hz from " << c.source << ")\n";
        failures += ok ? 0 : 1;
    }
}

int main()
{
    using dbziw::video::choose;
    const std::string sixty = "60", fifty = "50", other = "75";
    check(choose(nullptr, nullptr, nullptr), 50, "the disc's default", false, "nothing set: the disc's 50 Hz");
    check(choose("", nullptr, ""), 50, "the disc's default", false, "empty variables count as unset");
    check(choose(nullptr, nullptr, "60"), 60, "the launcher's default", false, "launcher: 60 Hz");
    check(choose(nullptr, &fifty, "60"), 50, "dbziw.ini", false, "the player's video_mode=50 beats the launcher");
    check(choose(nullptr, &sixty, nullptr), 60, "dbziw.ini", false, "the player's video_mode=60 without the launcher");
    check(choose("50", &sixty, "60"), 50, "PS2X_VIDEO_MODE", false, "a test's PS2X_VIDEO_MODE beats everything");
    check(choose("60", &fifty, nullptr), 60, "PS2X_VIDEO_MODE", false, "a test can ask for 60 Hz");
    check(choose(nullptr, &other, "60"), 50, "dbziw.ini", true, "video_mode=75: 50 Hz, reported");
    std::cout << (failures ? "video_mode_choice: FAIL\n" : "video_mode_choice: all checks passed\n");
    return failures ? 1 : 0;
}
