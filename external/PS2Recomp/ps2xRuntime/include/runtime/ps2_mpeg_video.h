// Shared MPEG-1/2 video decoder core (DW5's FFmpeg decoder, moved out of Kernel/Stubs/MPEG.cpp for issue 020).
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace ps2x::mpeg
{
    struct DecodedFrame
    {
        int width = 0;
        int height = 0;
        int repeatPict = 0;
        int64_t pts90k = -1;
        int pictType = 0; // FFmpeg AVPictureType: 1 I, 2 P, 3 B
        std::vector<uint8_t> rgba;
        std::vector<uint8_t> y, cb, cr; // filled only when setKeepYuv(true) and the stream is 4:2:0
    };

    class FfmpegDecoder
    {
    public:
        FfmpegDecoder();
        ~FfmpegDecoder();
        FfmpegDecoder(const FfmpegDecoder &) = delete;
        FfmpegDecoder &operator=(const FfmpegDecoder &) = delete;

        // Elementary stream in any pieces; FFmpeg's parser finds the pictures (DW5's PSS input).
        bool feed(const uint8_t *data, size_t size, std::deque<DecodedFrame> &frames, int64_t pts90k = -1, int64_t dts90k = -1);
        // Exactly one picture, with any headers before it (DBZIW's Sofdec input).
        bool decodePacket(const uint8_t *data, size_t size, std::deque<DecodedFrame> &frames);
        bool flush(std::deque<DecodedFrame> &frames);
        void reset();
        void setKeepYuv(bool keep);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // libmpeg's output layouts: macroblocks in column order (all of column 0 top to bottom, then column 1, ...).
    void toRgb32Macroblocks(const DecodedFrame &frame, std::vector<uint8_t> &out); // 1,024 bytes per macroblock
    void toRaw8Macroblocks(const DecodedFrame &frame, std::vector<uint8_t> &out);  // 384 bytes per macroblock
}
