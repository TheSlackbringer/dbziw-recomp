// Issue 020, pass criterion 4: a synthetic MPEG video stream (no game data) decodes through the Sofdec input to
// known output, fed by a DMA4 chain the way Sofdec feeds it (one REF tag per 2,048-byte sector, REFE on the last),
// including a no-data refill and Sofdec's save/restore of the IPU position between pictures.
#include "dbziw_movie_input.h"
#include "runtime/ps2_memory.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require(bool ok, const std::string &what)
    {
        if (!ok)
            throw std::runtime_error(what);
    }

    constexpr int kWidth = 128, kHeight = 96, kFrames = 9;
    constexpr uint8_t kLuma[kFrames] = {40, 70, 100, 130, 160, 190, 220, 90, 150};

    // Textured pictures around a known mean luma, GOP of 9 with B pictures, so decode order differs from display order.
    std::vector<uint8_t> encodeStream()
    {
        const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
        require(codec != nullptr, "FFmpeg has no MPEG-2 video encoder");
        AVCodecContext *c = avcodec_alloc_context3(codec);
        c->width = kWidth;
        c->height = kHeight;
        c->time_base = AVRational{1, 25};
        c->framerate = AVRational{25, 1};
        c->pix_fmt = AV_PIX_FMT_YUV420P;
        c->gop_size = kFrames;
        c->max_b_frames = 2;
        c->qmin = c->qmax = 2;
        require(avcodec_open2(c, codec, nullptr) == 0, "cannot open the MPEG-2 encoder");
        AVFrame *frame = av_frame_alloc();
        frame->format = c->pix_fmt;
        frame->width = kWidth;
        frame->height = kHeight;
        require(av_frame_get_buffer(frame, 0) == 0, "cannot allocate a frame");
        AVPacket *packet = av_packet_alloc();
        std::vector<uint8_t> stream;
        const auto drain = [&]
        {
            while (avcodec_receive_packet(c, packet) == 0)
            {
                stream.insert(stream.end(), packet->data, packet->data + packet->size);
                av_packet_unref(packet);
            }
        };
        for (int i = 0; i < kFrames; ++i)
        {
            require(av_frame_make_writable(frame) == 0, "frame not writable");
            for (int y = 0; y < kHeight; ++y)
                for (int x = 0; x < kWidth; ++x)
                    frame->data[0][y * frame->linesize[0] + x] =
                        static_cast<uint8_t>(kLuma[i] + ((x * 7 + y * 13 + i * 5) % 32) - 16);
            for (int y = 0; y < kHeight / 2; ++y)
            {
                std::memset(frame->data[1] + y * frame->linesize[1], 128, kWidth / 2);
                std::memset(frame->data[2] + y * frame->linesize[2], 128, kWidth / 2);
            }
            frame->pts = i;
            require(avcodec_send_frame(c, frame) == 0, "encoder rejected a frame");
            drain();
        }
        avcodec_send_frame(c, nullptr);
        drain();
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&c);
        for (int i = 0; i < 4; ++i) // Sofdec's end code (0x492c08): four sequence end codes
            stream.insert(stream.end(), {0x00, 0x00, 0x01, 0xB7});
        return stream;
    }

    struct Hook : PS2Memory::IpuInputHook
    {
        dbziw::movies::VideoInput *input = nullptr;
        uint32_t ipuBp() const override { return input->ipuBp(); }
        void ipuBclr(uint32_t bp) override { input->bclr(bp); }
    };

    constexpr uint32_t kRing = 0x00100000u, kTags = 0x00080000u, kSector = 2048u;
    constexpr uint32_t kChcr = 0x1000B400u, kMadr = 0x1000B410u, kQwc = 0x1000B420u, kTadr = 0x1000B430u;

    // Sofdec's feeder (FUN_001a0928/FUN_001a05c0): tags {addr << 32 | id << 28 | 128 QW}, REF then REFE.
    void startChain(PS2Memory &memory, uint32_t firstSector, uint32_t count)
    {
        uint8_t *ram = memory.getRDRAM();
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint64_t id = (i + 1u == count) ? 0u : 3u;
            const uint64_t tag = (static_cast<uint64_t>(kRing + (firstSector + i) * kSector) << 32u) | (id << 28u) | 128u;
            std::memcpy(ram + kTags + i * 16u, &tag, sizeof(tag));
            std::memset(ram + kTags + i * 16u + 8u, 0, 8u);
        }
        memory.write32(kQwc, 0u);
        memory.write32(kTadr, kTags);
        memory.write32(kChcr, 0x105u); // source chain, from memory, start
    }

    // Sofdec's save and restore around a picture request (FUN_001a0b68 / FUN_001a0c58), for a contiguous ring.
    void saveAndRestore(PS2Memory &memory)
    {
        const uint32_t bp = memory.read32(0x10002020u);
        const uint32_t held = ((bp >> 8u) & 0xFu) + ((bp >> 16u) & 3u);
        memory.write32(kChcr, memory.read32(kChcr) & ~0x100u);
        const uint32_t chcr = memory.read32(kChcr), madr = memory.read32(kMadr);
        const uint32_t qwc = memory.read32(kQwc), tadr = memory.read32(kTadr);
        memory.write32(0x10002000u, bp & 0x7Fu); // BCLR at the saved bit
        memory.write32(kMadr, madr - held * 16u);
        memory.write32(kTadr, tadr);
        memory.write32(kQwc, qwc + held);
        if (qwc + held != 0u)
            memory.write32(kChcr, chcr | 0x100u);
    }

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
        const uint32_t sectors = static_cast<uint32_t>((stream.size() + kSector - 1u) / kSector);
        require(sectors >= 2u, "synthetic stream shorter than two sectors (" + std::to_string(stream.size()) + " bytes)");
        std::memset(memory.getRDRAM() + kRing, 0, sectors * kSector);
        std::memcpy(memory.getRDRAM() + kRing, stream.data(), stream.size());

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
            saveAndRestore(memory);
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
