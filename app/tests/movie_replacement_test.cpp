// Issue 020, pass criterion 4: the synthetic MPEG stream (no game data) decoded through the libmpeg replacement
// itself (src/dbziw_movies.cpp) on the runtime's EE scheduler, driven the way Sofdec drives it: sceMpegInit,
// sceMpegCreate, its four callbacks registered with sceMpegAddCallback, then one RGB32 sceMpegGetPicture per picture
// with Sofdec's restore before and save after each request (FUN_001a1928). The callbacks do what Sofdec's do: no data
// feeds the rest of the ring (FUN_001a1428 -> FUN_001a0928), stop DMA saves the channel (FUN_001a14f8 -> FUN_001a0b68),
// restart DMA restores it (FUN_001a1520 -> FUN_001a0c58). Checks the pictures (display order, size, grey content) and
// the callback order: one no-data call when the first half of the ring runs dry, stop then restart around each picture.
#include "game_overrides.h"
#include "movie_test_stream.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ee_scheduler.h"
#include "runtime/ps2_memory.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// The function table the generated code normally provides (as ps2xTest/src/test_function_table.cpp does).
extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x00000000u;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = PS2_RAM_SIZE;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount =
    (g_ps2RecompiledFunctionTableEnd - g_ps2RecompiledFunctionTableBase) >> 2;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[g_ps2RecompiledFunctionTableSlotCount] = {};

namespace
{
    using namespace movie_test;

    // libmpeg's entry points in SLES_553.47 and the first instruction the replacement checks before it binds.
    constexpr uint32_t kMpegInit = 0x002905A0u, kMpegCreate = 0x0028F9F8u, kMpegReset = 0x00290738u;
    constexpr uint32_t kMpegAddCallback = 0x00290810u, kMpegGetPicture = 0x00290698u;
    constexpr uint32_t kMpegGetPictureRaw8 = 0x002906E0u, kMpegSetEnd = 0x00290728u;
    constexpr struct
    {
        uint32_t address, firstWord;
    } kLibmpeg[] = {{kMpegInit, 0x27BDFFF0u}, {kMpegCreate, 0x27BDFFB0u}, {kMpegReset, 0x27BDFFE0u},
                    {kMpegAddCallback, 0x2402000Cu}, {kMpegGetPicture, 0x27BDFFF0u},
                    {kMpegGetPictureRaw8, 0x27BDFFF0u}, {kMpegSetEnd, 0x8C830040u}};

    // Native stand-ins for Sofdec's code, at addresses outside the game's.
    constexpr uint32_t kDriverPc = 0x01F00000u, kNoDataPc = 0x01F00100u, kStopDmaPc = 0x01F00110u;
    constexpr uint32_t kRestartDmaPc = 0x01F00120u;
    constexpr uint32_t kMp = 0x00200000u, kWork = 0x00201000u, kWorkSize = 0x2000u, kPictureBuffer = 0x00300000u;
    constexpr uint32_t kMacroblocks = (kWidth / 16) * (kHeight / 16);

    struct Run
    {
        PS2Memory *memory = nullptr;
        uint32_t sectors = 0u, nextSector = 0u;
        int step = 0;
        bool saved = false;
        SavedChannel channel;
        std::string calls; // N no data, S stop DMA, R restart DMA, P picture returned
        std::vector<double> red;
        std::string error;
    } g_run;

    void returnTo(R5900Context *ctx, int32_t result)
    {
        SET_GPR_U32(ctx, 2, static_cast<uint32_t>(result));
        ctx->pc = getRegU32(ctx, 31);
    }

    void call(R5900Context *ctx, uint32_t function, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
    {
        SET_GPR_U32(ctx, 4, a0);
        SET_GPR_U32(ctx, 5, a1);
        SET_GPR_U32(ctx, 6, a2);
        SET_GPR_U32(ctx, 7, a3);
        SET_GPR_U32(ctx, 31, kDriverPc);
        ctx->pc = function;
    }

    void finish(R5900Context *ctx, PS2Runtime *runtime, const std::string &error)
    {
        g_run.error = error;
        ctx->pc = 0u;
        runtime->requestStop();
    }

    void noData(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        g_run.calls += 'N';
        if (g_run.nextSector < g_run.sectors)
        {
            startChain(*g_run.memory, g_run.nextSector, g_run.sectors - g_run.nextSector);
            g_run.nextSector = g_run.sectors;
        }
        returnTo(ctx, 1);
    }

    void stopDma(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        g_run.calls += 'S';
        g_run.channel = save(*g_run.memory);
        returnTo(ctx, 1);
    }

    void restartDma(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        g_run.calls += 'R';
        restore(*g_run.memory, g_run.channel);
        returnTo(ctx, 1);
    }

    // Mean red, green and blue of the RGB32 macroblocks (layout order does not matter for a mean).
    void meanColour(const uint8_t *rdram, double &r, double &g, double &b)
    {
        double sum[3] = {0.0, 0.0, 0.0};
        const uint32_t pixels = kMacroblocks * 256u;
        for (uint32_t i = 0; i < pixels; ++i)
            for (int c = 0; c < 3; ++c)
                sum[c] += rdram[kPictureBuffer + i * 4u + static_cast<uint32_t>(c)];
        r = sum[0] / pixels;
        g = sum[1] / pixels;
        b = sum[2] / pixels;
    }

    void driver(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const int32_t result = static_cast<int32_t>(getRegU32(ctx, 2));
        switch (g_run.step++)
        {
        case 0:
            return call(ctx, kMpegInit, 0u, 0u, 0u, 0u);
        case 1:
            return call(ctx, kMpegCreate, kMp, kWork, kWorkSize, 0u);
        case 2:
            if (result != 1)
                return finish(ctx, runtime, "sceMpegCreate returned " + std::to_string(result));
            return call(ctx, kMpegAddCallback, kMp, 1u, kNoDataPc, 0u);
        case 3:
            return call(ctx, kMpegAddCallback, kMp, 2u, kStopDmaPc, 0u);
        case 4:
            return call(ctx, kMpegAddCallback, kMp, 3u, kRestartDmaPc, 0u);
        case 5:
            g_run.nextSector = g_run.sectors / 2u;
            startChain(*g_run.memory, 0u, g_run.nextSector);
            return call(ctx, kMpegGetPicture, kMp, kPictureBuffer, kMacroblocks, 0u);
        default:
            break;
        }
        // After a picture request: Sofdec saves the channel, and restores it before the next request.
        g_run.channel = save(*g_run.memory);
        if (result == -1)
            return finish(ctx, runtime, "");
        if (result != 1)
            return finish(ctx, runtime, "sceMpegGetPicture returned " + std::to_string(result));
        g_run.calls += 'P';
        uint32_t width = 0u, height = 0u;
        std::memcpy(&width, rdram + kMp + 0u, sizeof(width));
        std::memcpy(&height, rdram + kMp + 4u, sizeof(height));
        if (width != static_cast<uint32_t>(kWidth) || height != static_cast<uint32_t>(kHeight))
            return finish(ctx, runtime, "sceMpeg reports " + std::to_string(width) + "x" + std::to_string(height));
        double r = 0.0, gr = 0.0, b = 0.0;
        meanColour(rdram, r, gr, b);
        if (std::abs(r - gr) > 4.0 || std::abs(r - b) > 4.0)
            return finish(ctx, runtime, "picture " + std::to_string(g_run.red.size()) + " is not grey");
        g_run.red.push_back(r);
        if (g_run.red.size() > static_cast<size_t>(kFrames))
            return finish(ctx, runtime, "more pictures than encoded");
        restore(*g_run.memory, g_run.channel);
        call(ctx, kMpegGetPicture, kMp, kPictureBuffer, kMacroblocks, 0u);
    }

    void testCallbackOrder()
    {
        auto runtime = std::make_unique<PS2Runtime>();
        PS2Memory &memory = runtime->memory();
        require(memory.initialize(), "PS2Memory::initialize failed");
        memory.write32(0x1000E000u, 1u); // D_CTRL DMAE
        uint8_t *rdram = memory.getRDRAM();
        for (const auto &entry : kLibmpeg)
            std::memcpy(rdram + entry.address, &entry.firstWord, sizeof(entry.firstWord));
        ps2_game_overrides::applyMatching(*runtime, "SLES_553.47", 0u, 0u, false);
        for (const auto &entry : kLibmpeg)
            require(runtime->hasFunction(entry.address), "the replacement did not bind its libmpeg entry points");

        runtime->registerFunction(kDriverPc, driver);
        runtime->registerFunction(kNoDataPc, noData);
        runtime->registerFunction(kStopDmaPc, stopDma);
        runtime->registerFunction(kRestartDmaPc, restartDma);

        g_run = Run{};
        g_run.memory = &memory;
        g_run.sectors = loadRing(memory, encodeStream());

        R5900Context main{};
        main.pc = kDriverPc;
        SET_GPR_U32(&main, 29, 0x01E00000u);
        runtime->eeScheduler().reset(rdram, main);
        runtime->eeScheduler().run();

        require(g_run.error.empty(), g_run.error);
        require(g_run.red.size() == static_cast<size_t>(kFrames),
                "decoded " + std::to_string(g_run.red.size()) + " of " + std::to_string(kFrames) + " pictures");
        // Display order: the pictures' brightness ranks as the encoded luma ranks.
        for (int i = 0; i < kFrames; ++i)
            for (int j = 0; j < kFrames; ++j)
                if (kLuma[i] < kLuma[j])
                    require(g_run.red[i] < g_run.red[j], "picture " + std::to_string(i) + " is out of display order");
        std::string expected;
        for (int i = 0; i < kFrames; ++i)
            expected += "SRP";
        std::string withoutNoData = g_run.calls;
        withoutNoData.erase(std::remove(withoutNoData.begin(), withoutNoData.end(), 'N'), withoutNoData.end());
        require(withoutNoData == expected, "callback order " + g_run.calls + ", expected stop, restart, picture each time");
        require(std::count(g_run.calls.begin(), g_run.calls.end(), 'N') == 1,
                "expected one no-data call, got " + g_run.calls);
        std::printf("  %u sectors, %d pictures, calls %s\n", g_run.sectors, kFrames, g_run.calls.c_str());
    }
}

int main()
{
    try
    {
        testCallbackOrder();
        std::printf("PASS: movie_replacement\n");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::printf("FAIL: movie_replacement: %s\n", error.what());
        return 1;
    }
}
