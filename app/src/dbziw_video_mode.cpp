// DBZIW 019: the player's video mode, 60 Hz or 50 Hz.
//
// The PAL disc starts at 50 Hz: the game's start-up block at 0x420300 (copied to the stack by 0x1491f0 and handed to
// the engine start 0x143ff0, which calls SetGsCrt and sets the game's frame rate from it) holds video mode 3 (PAL).
// 60 Hz is the game's own option, hidden behind L1+L2+R1+R2 on the language screen: its "Yes" restarts the display
// with the block at 0x47d430, which is the same block with mode 2 (NTSC timing, 59.94 fields/s); the dialog's states
// (0x2d4f60, 0x2d4c40) change nothing else. With video_mode=60 this override gives the start-up block mode 2 before
// the game reads it, so the game's own code starts in its 60 Hz mode, as after "Yes".
//
// The setting (dbziw_video_mode.h): PS2X_VIDEO_MODE (test runs), video_mode in the settings file (dbziw.ini), the
// launcher's default (PS2X_VIDEO_MODE_DEFAULT=60, set when it starts the game; it writes no file), else the disc's 50 Hz.
#include "dbziw_video_mode.h"
#include "game_overrides.h"
#include "ps2_runtime.h"
#include "runtime/ps2_memory.h"
#include "runtime/ps2_settings.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    constexpr uint32_t kStartBlock = 0x420300u;  // start-up display block: mode 3 (PAL)
    constexpr uint32_t kSwitchBlock = 0x47d430u; // the block the 60 Hz "Yes" uses: mode 2 (NTSC timing)
    constexpr uint8_t kPalBlock[16] = {3, 0, 0, 0, 1, 1, 0, 2, 0xC0, 1, 0x58, 2, 0xC2, 1, 0x80, 0};

    void applyVideoMode(PS2Runtime &runtime)
    {
        std::string ini;
        const bool hasIni = ps2x::settings::loadText("video_mode", ini);
        const dbziw::video::Choice choice =
            dbziw::video::choose(std::getenv("PS2X_VIDEO_MODE"), hasIni ? &ini : nullptr, std::getenv("PS2X_VIDEO_MODE_DEFAULT"));
        unsigned hz = choice.hz;
        if (choice.invalid)
            std::fprintf(stderr, "[video] the video mode from %s is neither 60 nor 50: the game starts at 50 Hz\n", choice.source);

        uint8_t *rdram = runtime.memory().getRDRAM();
        bool expected = std::memcmp(rdram + kStartBlock, kPalBlock, sizeof(kPalBlock)) == 0 &&
                        rdram[kSwitchBlock] == 2u &&
                        std::memcmp(rdram + kSwitchBlock + 1u, kPalBlock + 1, sizeof(kPalBlock) - 1u) == 0;
        if (hz == 60u && !expected)
        {
            std::fprintf(stderr, "[video] the start-up blocks are not as expected (0x%x, 0x%x): the game starts at 50 Hz\n",
                         kStartBlock, kSwitchBlock);
            hz = 50u;
        }
        if (hz == 60u)
            rdram[kStartBlock] = 2u; // the 60 Hz "Yes" mode, before the game reads the block
        std::fprintf(stderr, "[video] mode=%u Hz (source: %s)\n", hz, choice.source);
    }
}

PS2_REGISTER_GAME_OVERRIDE("DBZIW video mode (50/60 Hz)", "SLES_553.47", 0u, 0u, &applyVideoMode)
