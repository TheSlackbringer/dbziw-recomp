// DBZIW 019: the game's PAL50 <-> PAL60 switch. SetGsCrt (the syscall the game's libgraph makes) sets the scheduler's
// field rate and SMODE1.CMOD, and the scanout takes the TV system from CMOD; modes other than NTSC/PAL keep the rate.
#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include "runtime/gs/gs_scanout_mode.h"
#include "ps2_syscalls.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace
{
void require(bool ok, const char *why)
{
    if (!ok)
    {
        std::cerr << "FAILED: " << why << std::endl;
        throw std::runtime_error(why);
    }
}

void reg(R5900Context &c, unsigned n, uint32_t v)
{
    c.r[n] = _mm_cvtsi32_si128(static_cast<int>(v));
}

constexpr uint64_t kPalCycles = EeScheduler::kEeClockHz / 50u;
constexpr uint64_t kNtscCycles = (EeScheduler::kEeClockHz * 1001u + 59999u) / 60000u;

uint32_t cmod(PS2Runtime &runtime)
{
    return ps2x::gs::smode1Cmod(runtime.memory().gs().smode1);
}

void setCrt(PS2Runtime &runtime, std::vector<uint8_t> &rdram, uint32_t interlaced, uint32_t mode, uint32_t frame)
{
    R5900Context ctx{};
    reg(ctx, 4, interlaced);
    reg(ctx, 5, mode);
    reg(ctx, 6, frame);
    reg(ctx, 31, 0x00100448u);
    ps2_syscalls::GsSetCrt(rdram.data(), &ctx, &runtime);
}
} // namespace

int main()
{
    try
    {
        PS2Runtime runtime;
        std::vector<uint8_t> rdram(0x2000000u, 0);
        EeScheduler &scheduler = runtime.eeScheduler();

        // 1. The scheduler: mode 3 = PAL 50 fields/s, mode 2 = NTSC 59.94, anything else keeps the rate.
        scheduler.setCrtMode(3u);
        require(scheduler.palVideoMode() && scheduler.fieldPeriodCycles() == kPalCycles, "mode 3: PAL field period");
        scheduler.setCrtMode(2u);
        require(!scheduler.palVideoMode() && scheduler.fieldPeriodCycles() == kNtscCycles, "mode 2: NTSC field period");
        scheduler.setCrtMode(0x50u);
        require(!scheduler.palVideoMode() && scheduler.fieldPeriodCycles() == kNtscCycles, "mode 0x50 keeps the NTSC rate");
        scheduler.setCrtMode(3u);
        scheduler.setCrtMode(0x1Au);
        require(scheduler.palVideoMode() && scheduler.fieldPeriodCycles() == kPalCycles, "mode 0x1A keeps the PAL rate");

        // 2. The syscall, as the game makes it (PAL at start-up, then NTSC after "Yes", then back): rate and CMOD.
        runtime.memory().gs().smode1 = 0u;
        setCrt(runtime, rdram, 1u, 3u, 0u);
        require(scheduler.palVideoMode() && cmod(runtime) == 3u, "SetGsCrt(1, 3, 0): PAL rate, CMOD 3");
        require((runtime.memory().gs().smode2 & 3u) == 1u, "SetGsCrt keeps SMODE2 interlaced, field mode");
        setCrt(runtime, rdram, 1u, 2u, 0u);
        require(!scheduler.palVideoMode() && cmod(runtime) == 2u, "SetGsCrt(1, 2, 0): NTSC rate, CMOD 2");
        runtime.memory().gs().smode1 |= 0x0000000000000400ull; // other SMODE1 bits stay as they are
        setCrt(runtime, rdram, 1u, 3u, 0u);
        require(scheduler.palVideoMode() && cmod(runtime) == 3u && (runtime.memory().gs().smode1 & 0x400ull) != 0u,
                "SetGsCrt(1, 3, 0) again: PAL, CMOD 3, other bits kept");
        setCrt(runtime, rdram, 0u, 0x50u, 0u);
        require(scheduler.palVideoMode() && cmod(runtime) == 0u, "SetGsCrt(0, 0x50, 0): rate kept, CMOD 0");

        // 3. The scanout's choice: CMOD first; the old height guess only when CMOD is neither 2 nor 3.
        require(!ps2x::gs::scanoutIsPal(2ull << 13u, true), "CMOD 2 with a tall picture (448 lines): NTSC");
        require(ps2x::gs::scanoutIsPal(3ull << 13u, false), "CMOD 3: PAL");
        require(ps2x::gs::scanoutIsPal(0u, true), "CMOD 0, tall picture: PAL (height guess)");
        require(!ps2x::gs::scanoutIsPal(0u, false), "CMOD 0, short picture: NTSC (height guess)");

        std::cout << "crt_mode_switch: all checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
