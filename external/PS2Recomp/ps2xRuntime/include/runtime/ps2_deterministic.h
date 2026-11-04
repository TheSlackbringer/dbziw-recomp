#pragma once
// DBZIW issue 022 (option A, 2026-11-03): PS2X_DETERMINISTIC=1, a test and replay mode in which two runs with the
// same input reach the same guest state field for field. Every guest-visible time then comes from the guest's own
// EE cycle count instead of the PC: fields start at their guest cycle only (no jump to the wall-clock field time),
// the IOP runs on EE cycles / 8, the GS runs inline and the device worker stays off (their completion times follow
// the PC), the idle wait picks its next deadline by guest cycles, and host dates (file times, the CD clock) are one
// fixed date. The PC still waits for real time when the game is ahead; when the PC is slower than the game, the game
// runs slower than real time instead of dropping fields. Normal play never sets it.
#include <cstdlib>
#include <ctime>

namespace ps2x
{
    inline bool deterministicMode() noexcept
    {
        static const bool on = []
        {
            const char *value = std::getenv("PS2X_DETERMINISTIC");
            return value && value[0] == '1' && value[1] == '\0';
        }();
        return on;
    }

    // 2026-01-01 00:00:00 UTC: the date every host time reads as in the deterministic mode.
    inline constexpr std::time_t kDeterministicDate = 1767225600;
}
