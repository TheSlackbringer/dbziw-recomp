// Issue 020, pass criterion 4: the synthetic movie and Sofdec's DMA4 feeding, shared by the movie CTests.
// A textured MPEG-2 stream (no game data) and Sofdec's ring: one REF tag per 2,048-byte sector, REFE on the last
// (FUN_001a0928/FUN_001a05c0), with its save (FUN_001a0b68) and restore (FUN_001a0c58) of the IPU input position.
#pragma once

#include "runtime/ps2_memory.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace movie_test
{
    inline void require(bool ok, const std::string &what)
    {
        if (!ok)
            throw std::runtime_error(what);
    }

    constexpr int kWidth = 128, kHeight = 96, kFrames = 9;
    constexpr uint8_t kLuma[kFrames] = {40, 70, 100, 130, 160, 190, 220, 90, 150};

    // Textured pictures around a known mean luma, GOP of 9 with B pictures, so decode order differs from display order.
    inline std::vector<uint8_t> encodeStream()
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

    constexpr uint32_t kRing = 0x00100000u, kTags = 0x00080000u, kSector = 2048u;
    constexpr uint32_t kChcr = 0x1000B400u, kMadr = 0x1000B410u, kQwc = 0x1000B420u, kTadr = 0x1000B430u;
    constexpr uint32_t kIpuCmd = 0x10002000u, kIpuBp = 0x10002020u;

    // Copies the stream into the ring; returns its length in sectors.
    inline uint32_t loadRing(PS2Memory &memory, const std::vector<uint8_t> &stream)
    {
        const uint32_t sectors = static_cast<uint32_t>((stream.size() + kSector - 1u) / kSector);
        require(sectors >= 2u, "synthetic stream shorter than two sectors (" + std::to_string(stream.size()) + " bytes)");
        std::memset(memory.getRDRAM() + kRing, 0, sectors * kSector);
        std::memcpy(memory.getRDRAM() + kRing, stream.data(), stream.size());
        return sectors;
    }

    // Sofdec's feeder (FUN_001a0928/FUN_001a05c0): tags {addr << 32 | id << 28 | 128 QW}, REF then REFE.
    inline void startChain(PS2Memory &memory, uint32_t firstSector, uint32_t count)
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

    struct SavedChannel
    {
        uint32_t chcr = 0u, madr = 0u, qwc = 0u, tadr = 0u, bp = 0u;
    };

    // Sofdec's save (FUN_001a0b68): stop DMA4, keep its registers and IPU_BP.
    inline SavedChannel save(PS2Memory &memory)
    {
        memory.write32(kChcr, memory.read32(kChcr) & ~0x100u);
        SavedChannel saved;
        saved.chcr = memory.read32(kChcr);
        saved.madr = memory.read32(kMadr);
        saved.qwc = memory.read32(kQwc);
        saved.tadr = memory.read32(kTadr);
        saved.bp = memory.read32(kIpuBp);
        return saved;
    }

    // Sofdec's restore (FUN_001a0c58) within one sector of a contiguous ring: BCLR at the saved bit, move DMA4 back
    // over the quadwords the IPU held (IFC + FP) and restart it.
    inline void restore(PS2Memory &memory, const SavedChannel &saved)
    {
        const uint32_t held = ((saved.bp >> 8u) & 0xFu) + ((saved.bp >> 16u) & 3u);
        memory.write32(kIpuCmd, saved.bp & 0x7Fu);
        memory.write32(kMadr, saved.madr - held * 16u);
        memory.write32(kTadr, saved.tadr);
        memory.write32(kQwc, saved.qwc + held);
        if (saved.qwc + held != 0u)
            memory.write32(kChcr, saved.chcr | 0x100u);
    }
}
