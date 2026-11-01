// Issue 020, pass criterion 4: a synthetic MPEG video stream (no game data) decodes through the Sofdec input to
// known output, fed by a DMA4 chain the way Sofdec feeds it (one REF tag per 2,048-byte sector, REFE on the last),
// including a no-data refill and Sofdec's save/restore of the IPU position between pictures.
#include "dbziw_movie_input.h"
#include "movie_test_stream.h"
#include "runtime/ps2_memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace movie_test;

    struct Hook : PS2Memory::IpuInputHook
    {
        dbziw::movies::VideoInput *input = nullptr;
        uint32_t ipuBp() const override { return input->ipuBp(); }
        void ipuBclr(uint32_t bp) override { input->bclr(bp); }
    };

    double meanLuma(const ps2x::mpeg::DecodedFrame &frame)
    {
        double sum = 0.0;
        for (uint8_t v : frame.y)
            sum += v;
        return frame.y.empty() ? -1.0 : sum / static_cast<double>(frame.y.size());
    }

    void testDecodeThroughDma4()
    {
        const std::vector<uint8_t> stream = encodeStream();
        PS2Memory memory;
        require(memory.initialize(), "PS2Memory::initialize failed");
        memory.write32(0x1000E000u, 1u); // D_CTRL DMAE
        const uint32_t sectors = loadRing(memory, stream);

        dbziw::movies::VideoInput input;
        Hook hook;
        hook.input = &input;
        memory.setIpuInputHook(&hook);

        const uint32_t firstBatch = sectors / 2u;
        startChain(memory, 0u, firstBatch);
        uint32_t nextSector = firstBatch, noData = 0u, pictures = 0u, maxHeld = 0u;
        for (int guard = 0; guard < 1000000; ++guard)
        {
            ps2x::mpeg::DecodedFrame frame;
            const auto result = input.next(frame);
            if (result == dbziw::movies::VideoInput::Result::Ended)
                break;
            if (result == dbziw::movies::VideoInput::Result::NeedData)
            {
                if (dbziw::movies::pullDma4Quadword(memory, input))
                    continue;
                // Sofdec's no-data callback: feed the rest of the ring.
                ++noData;
                require(nextSector < sectors, "decoder asked for data after the whole stream was fed");
                startChain(memory, nextSector, sectors - nextSector);
                nextSector = sectors;
                continue;
            }
            require(pictures < kFrames, "more pictures than encoded");
            require(frame.width == kWidth && frame.height == kHeight,
                    "picture " + std::to_string(pictures) + " is " + std::to_string(frame.width) + "x" + std::to_string(frame.height));
            const double luma = meanLuma(frame);
            require(luma > kLuma[pictures] - 3.0 && luma < kLuma[pictures] + 3.0,
                    "picture " + std::to_string(pictures) + " (display order) has luma " + std::to_string(luma) +
                        ", expected " + std::to_string(kLuma[pictures]));
            maxHeld = std::max(maxHeld, input.unusedQuadwords());
            require(input.unusedQuadwords() <= 10u, "more input held than the IPU can hold");
            ++pictures;
            restore(memory, save(memory));
        }
        require(pictures == kFrames, "decoded " + std::to_string(pictures) + " of " + std::to_string(kFrames) + " pictures");
        require(noData == 1u, "expected exactly one no-data refill, got " + std::to_string(noData));
        std::printf("  stream %zu bytes in %u sectors, %u pictures, %u no-data refill, at most %u quadwords held\n",
                    stream.size(), sectors, pictures, noData, maxHeld);
        memory.setIpuInputHook(nullptr);
    }

    void testMacroblockLayout()
    {
        // 32x32: four macroblocks, colour per quadrant. libmpeg's order is column by column.
        ps2x::mpeg::DecodedFrame frame;
        frame.width = frame.height = 32;
        frame.rgba.resize(32u * 32u * 4u);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
            {
                const uint8_t quadrant = static_cast<uint8_t>((y / 16) * 2 + (x / 16)); // 0 TL, 1 TR, 2 BL, 3 BR
                uint8_t *p = frame.rgba.data() + (y * 32 + x) * 4;
                p[0] = static_cast<uint8_t>(10 + quadrant);
                p[1] = static_cast<uint8_t>(x);
                p[2] = static_cast<uint8_t>(y);
                p[3] = 0xFF;
            }
        std::vector<uint8_t> out;
        ps2x::mpeg::toRgb32Macroblocks(frame, out);
        require(out.size() == 4u * 1024u, "RGB32 output size");
        const uint8_t expectedQuadrant[4] = {0, 2, 1, 3}; // MB 0 TL, MB 1 BL, MB 2 TR, MB 3 BR
        for (int mb = 0; mb < 4; ++mb)
        {
            const uint8_t *p = out.data() + mb * 1024 + (5 * 16 + 7) * 4; // pixel (7, 5) inside the macroblock
            require(p[0] == 10 + expectedQuadrant[mb], "RGB32 macroblock " + std::to_string(mb) + " holds the wrong quadrant");
            require(p[3] == 0x80u, "RGB32 alpha is not 0x80");
        }
    }
}

int main()
{
    try
    {
        testMacroblockLayout();
        testDecodeThroughDma4();
        std::printf("PASS: movie_input\n");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::printf("FAIL: movie_input: %s\n", error.what());
        return 1;
    }
}
