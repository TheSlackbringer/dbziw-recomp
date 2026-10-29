// Shared MPEG-1/2 video decoder core: DW5's FFmpeg decoder, moved out of Kernel/Stubs/MPEG.cpp so DW5's libmpeg
// HLE and DBZIW's Sofdec input (issue 020) use one decoder. Provenance: the class as vendored from dw5-ps2 at the fork
// (DW5 438a3de, last changed there in 543a551 "Fix #037"), moved unchanged apart from the namespace and the
// macroblock helpers; decoding is FFmpeg's (libavcodec, LGPL).
#include "runtime/ps2_mpeg_video.h"
#include "ps2_log.h"

#if !defined(PS2X_HAS_FFMPEG)
#define PS2X_HAS_FFMPEG 1
#endif

#if PS2X_HAS_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
}
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>

namespace ps2x::mpeg
{
    namespace
    {
#if PS2X_HAS_FFMPEG
        std::string ffmpegErrorString(int err)
        {
            std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
            if (av_strerror(err, buffer.data(), buffer.size()) < 0)
            {
                return "unknown FFmpeg error";
            }
            return std::string(buffer.data());
        }

        void configureFfmpegLogLevel()
        {
            static std::once_flag s_once;
            std::call_once(s_once, []
                           {
#if AGRESSIVE_LOGS
                               av_log_set_level(AV_LOG_WARNING);
#else
                               av_log_set_level(AV_LOG_ERROR);
#endif
                           });
        }

        class DecoderImpl
        {
        public:
            DecoderImpl() = default;

            ~DecoderImpl()
            {
                reset();
            }

            DecoderImpl(const DecoderImpl &) = delete;
            DecoderImpl &operator=(const DecoderImpl &) = delete;

            bool feed(const uint8_t *data, size_t size, std::deque<DecodedFrame> &frames, int64_t pts90k = -1, int64_t dts90k = -1)
            {
                if (!data || size == 0)
                {
                    return true;
                }

                if (!ensureInitialized())
                {
                    return false;
                }

                static uint32_t s_feedLogCount = 0u;
                const bool shouldLog = (s_feedLogCount < 32u);
                if (shouldLog)
                    ++s_feedLogCount;

                size_t totalParsed = 0u;
                size_t totalPacketsSent = 0u;
                size_t framesBefore = frames.size();

                const uint8_t *cursor = data;
                size_t remaining = size;
                int64_t parserPts = pts90k >= 0 ? pts90k : AV_NOPTS_VALUE;
                int64_t parserDts = dts90k >= 0 ? dts90k : AV_NOPTS_VALUE;
                while (remaining > 0)
                {
                    uint8_t *packetData = nullptr;
                    int packetSize = 0;
                    const int chunk = static_cast<int>(std::min<size_t>(
                        remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
                    const int used = av_parser_parse2(
                        m_parser,
                        m_codecCtx,
                        &packetData,
                        &packetSize,
                        cursor,
                        chunk,
                        parserPts,
                        parserDts,
                        0);
                    if (used < 0)
                    {
                        std::cerr << "[MPEG] parser failed: " << ffmpegErrorString(used) << std::endl;
                        return false;
                    }
                    if (used == 0 && packetSize == 0)
                    {
                        break;
                    }

                    totalParsed += static_cast<size_t>(used);
                    cursor += used;
                    remaining -= static_cast<size_t>(used);

                    if (used > 0)
                    {
                        parserPts = AV_NOPTS_VALUE;
                        parserDts = AV_NOPTS_VALUE;
                    }

                    if (packetSize > 0)
                    {
                        ++totalPacketsSent;
                        if (!sendPacket(packetData, static_cast<size_t>(packetSize), frames, m_parser->pts, m_parser->dts))
                        {
                            return false;
                        }
                    }
                }

                if (shouldLog)
                {
                    PS2_IF_AGRESSIVE_LOGS({
                        std::cerr << "[MPEG:feed] inSize=" << size
                                  << " parsed=" << totalParsed
                                  << " packets=" << totalPacketsSent
                                  << " newFrames=" << (frames.size() - framesBefore)
                                  << " totalFrames=" << frames.size()
                                  << std::endl;
                    });
                }

                return true;
            }

            bool flush(std::deque<DecodedFrame> &frames)
            {
                if (!m_initialized || m_drained)
                {
                    return true;
                }

                if (m_parser)
                {
                    uint8_t *packetData = nullptr;
                    int packetSize = 0;
                    const int used = av_parser_parse2(
                        m_parser,
                        m_codecCtx,
                        &packetData,
                        &packetSize,
                        nullptr,
                        0,
                        AV_NOPTS_VALUE,
                        AV_NOPTS_VALUE,
                        0);
                    (void)used;
                    if (packetSize > 0 && !sendPacket(packetData, static_cast<size_t>(packetSize), frames, m_parser->pts, m_parser->dts))
                    {
                        return false;
                    }
                }

                const int sendRet = avcodec_send_packet(m_codecCtx, nullptr);
                if (sendRet < 0 && sendRet != AVERROR_EOF)
                {
                    std::cerr << "[MPEG] decoder flush failed: " << ffmpegErrorString(sendRet) << std::endl;
                    return false;
                }

                const bool ok = receiveFrames(frames);
                m_drained = true;
                return ok;
            }

            void reset()
            {
                if (m_swsCtx)
                {
                    sws_freeContext(m_swsCtx);
                    m_swsCtx = nullptr;
                }
                if (m_frame)
                {
                    av_frame_free(&m_frame);
                }
                if (m_packet)
                {
                    av_packet_free(&m_packet);
                }
                if (m_codecCtx)
                {
                    avcodec_free_context(&m_codecCtx);
                }
                if (m_parser)
                {
                    av_parser_close(m_parser);
                    m_parser = nullptr;
                }

                m_swsWidth = 0;
                m_swsHeight = 0;
                m_swsFormat = AV_PIX_FMT_NONE;
                m_initialized = false;
                m_drained = false;
            }

            // Issue 020: the caller has already split the stream into one picture per packet.
            bool decodePacket(const uint8_t *data, size_t size, std::deque<DecodedFrame> &frames)
            {
                if (!ensureInitialized())
                {
                    return false;
                }
                m_drained = false;
                return sendPacket(data, size, frames);
            }

            void setKeepYuv(bool keep)
            {
                m_keepYuv = keep;
            }

        private:
            bool ensureInitialized()
            {
                if (m_initialized)
                {
                    return true;
                }

                configureFfmpegLogLevel();

                const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
                if (!codec)
                {
                    std::cerr << "[MPEG] FFmpeg MPEG-2 decoder not found." << std::endl;
                    return false;
                }

                m_parser = av_parser_init(AV_CODEC_ID_MPEG2VIDEO);
                if (!m_parser)
                {
                    std::cerr << "[MPEG] FFmpeg MPEG-video parser not found." << std::endl;
                    return false;
                }

                m_codecCtx = avcodec_alloc_context3(codec);
                m_frame = av_frame_alloc();
                m_packet = av_packet_alloc();
                if (!m_codecCtx || !m_frame || !m_packet)
                {
                    std::cerr << "[MPEG] failed to allocate FFmpeg decoder state." << std::endl;
                    reset();
                    return false;
                }

                m_codecCtx->thread_count = 1;
                m_codecCtx->pkt_timebase = AVRational{1, 90000};
                // feedElementaryStream() does not create the decoder until a valid
                // MPEG sequence header has been found.  Dropping non-key pictures
                // here therefore throws away real presentation frames and makes
                // movies finish early once the EE is fast enough to drain them.
                m_codecCtx->skip_frame = AVDISCARD_DEFAULT;
                m_codecCtx->err_recognition = 0;
                const int ret = avcodec_open2(m_codecCtx, codec, nullptr);
                if (ret < 0)
                {
                    std::cerr << "[MPEG] failed to open MPEG decoder: " << ffmpegErrorString(ret) << std::endl;
                    reset();
                    return false;
                }

                m_initialized = true;
                m_drained = false;
                return true;
            }

            bool sendPacket(const uint8_t *data,
                            size_t size,
                            std::deque<DecodedFrame> &frames,
                            int64_t pts = AV_NOPTS_VALUE,
                            int64_t dts = AV_NOPTS_VALUE)
            {
                if (!data || size == 0)
                {
                    return true;
                }

                av_packet_unref(m_packet);
                const int allocRet = av_new_packet(m_packet, static_cast<int>(size));
                if (allocRet < 0)
                {
                    std::cerr << "[MPEG] failed to allocate packet: " << ffmpegErrorString(allocRet) << std::endl;
                    return false;
                }
                std::memcpy(m_packet->data, data, size);
                m_packet->pts = pts;
                m_packet->dts = dts;

                int ret = avcodec_send_packet(m_codecCtx, m_packet);
                if (ret == AVERROR(EAGAIN))
                {
                    if (!receiveFrames(frames))
                    {
                        av_packet_unref(m_packet);
                        return false;
                    }
                    ret = avcodec_send_packet(m_codecCtx, m_packet);
                }
                av_packet_unref(m_packet);
                if (ret < 0 && ret != AVERROR(EAGAIN))
                {
                    static uint32_t s_rejectedPacketLogCount = 0u;
                    if (s_rejectedPacketLogCount < 32u)
                    {
                        std::cerr << "[MPEG] decoder rejected packet, dropping: "
                                  << ffmpegErrorString(ret) << std::endl;
                        ++s_rejectedPacketLogCount;
                    }
                    return true;
                }

                return receiveFrames(frames);
            }

            bool receiveFrames(std::deque<DecodedFrame> &frames)
            {
                while (true)
                {
                    const int ret = avcodec_receive_frame(m_codecCtx, m_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    {
                        return true;
                    }
                    if (ret < 0)
                    {
                        static uint32_t s_receiveErrorLogCount = 0u;
                        if (s_receiveErrorLogCount < 32u)
                        {
                            std::cerr << "[MPEG] decoder receive failed, dropping: "
                                      << ffmpegErrorString(ret) << std::endl;
                            ++s_receiveErrorLogCount;
                        }
                        return true;
                    }

                    if (!convertFrame(frames))
                    {
                        av_frame_unref(m_frame);
                        return false;
                    }
                    av_frame_unref(m_frame);
                }
            }

            bool convertFrame(std::deque<DecodedFrame> &frames)
            {
                const int width = m_frame->width;
                const int height = m_frame->height;
                const AVPixelFormat srcFormat = static_cast<AVPixelFormat>(m_frame->format);
                if (width <= 0 || height <= 0 || srcFormat == AV_PIX_FMT_NONE)
                {
                    return false;
                }

                if (!m_swsCtx ||
                    m_swsWidth != width ||
                    m_swsHeight != height ||
                    m_swsFormat != srcFormat)
                {
                    if (m_swsCtx)
                    {
                        sws_freeContext(m_swsCtx);
                        m_swsCtx = nullptr;
                    }
                    m_swsCtx = sws_getContext(
                        width,
                        height,
                        srcFormat,
                        width,
                        height,
                        AV_PIX_FMT_RGBA,
                        SWS_BILINEAR,
                        nullptr,
                        nullptr,
                        nullptr);
                    if (!m_swsCtx)
                    {
                        std::cerr << "[MPEG] failed to create FFmpeg scaler." << std::endl;
                        return false;
                    }
                    m_swsWidth = width;
                    m_swsHeight = height;
                    m_swsFormat = srcFormat;
                }

                DecodedFrame decoded;
                decoded.width = width;
                decoded.height = height;
                decoded.repeatPict = std::max(0, m_frame->repeat_pict);
                decoded.pts90k = m_frame->best_effort_timestamp != AV_NOPTS_VALUE
                                     ? m_frame->best_effort_timestamp
                                     : -1;
                decoded.rgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

                uint8_t *dstData[4] = {decoded.rgba.data(), nullptr, nullptr, nullptr};
                int dstLinesize[4] = {width * 4, 0, 0, 0};
                const int scaledRows = sws_scale(
                    m_swsCtx,
                    m_frame->data,
                    m_frame->linesize,
                    0,
                    height,
                    dstData,
                    dstLinesize);
                if (scaledRows <= 0)
                {
                    std::cerr << "[MPEG] FFmpeg scaler produced no rows." << std::endl;
                    return false;
                }

                decoded.pictType = static_cast<int>(m_frame->pict_type);
                if (m_keepYuv && srcFormat == AV_PIX_FMT_YUV420P)
                {
                    const auto plane = [&](int index, int planeWidth, int planeHeight, std::vector<uint8_t> &out)
                    {
                        out.resize(static_cast<size_t>(planeWidth) * static_cast<size_t>(planeHeight));
                        for (int row = 0; row < planeHeight; ++row)
                            std::memcpy(out.data() + static_cast<size_t>(row) * planeWidth,
                                        m_frame->data[index] + static_cast<ptrdiff_t>(row) * m_frame->linesize[index],
                                        static_cast<size_t>(planeWidth));
                    };
                    plane(0, width, height, decoded.y);
                    plane(1, (width + 1) / 2, (height + 1) / 2, decoded.cb);
                    plane(2, (width + 1) / 2, (height + 1) / 2, decoded.cr);
                }

                frames.push_back(std::move(decoded));
                return true;
            }

            AVCodecParserContext *m_parser = nullptr;
            AVCodecContext *m_codecCtx = nullptr;
            AVFrame *m_frame = nullptr;
            AVPacket *m_packet = nullptr;
            SwsContext *m_swsCtx = nullptr;
            int m_swsWidth = 0;
            int m_swsHeight = 0;
            AVPixelFormat m_swsFormat = AV_PIX_FMT_NONE;
            bool m_initialized = false;
            bool m_drained = false;
            bool m_keepYuv = false;
        };
#else
        // TODO
        class DecoderImpl
        {
        public:
            bool feed(const uint8_t *, size_t, std::deque<DecodedFrame> &, int64_t = -1, int64_t = -1)
            {
                static bool s_warnedNoFfmpeg = false;
                if (!s_warnedNoFfmpeg)
                {
                    std::cerr << "[MPEG] runtime built without FFmpeg; MPEG video decode is disabled." << std::endl;
                    s_warnedNoFfmpeg = true;
                }
                return false;
            }

            bool flush(std::deque<DecodedFrame> &)
            {
                return true;
            }

            void reset() {}

            bool decodePacket(const uint8_t *, size_t, std::deque<DecodedFrame> &)
            {
                return false;
            }

            void setKeepYuv(bool) {}
        };
#endif
    }

    struct FfmpegDecoder::Impl : DecoderImpl
    {
    };

    FfmpegDecoder::FfmpegDecoder() : m_impl(std::make_unique<Impl>()) {}
    FfmpegDecoder::~FfmpegDecoder() = default;

    bool FfmpegDecoder::feed(const uint8_t *data, size_t size, std::deque<DecodedFrame> &frames, int64_t pts90k, int64_t dts90k)
    {
        return m_impl->feed(data, size, frames, pts90k, dts90k);
    }

    bool FfmpegDecoder::decodePacket(const uint8_t *data, size_t size, std::deque<DecodedFrame> &frames)
    {
        return m_impl->decodePacket(data, size, frames);
    }

    bool FfmpegDecoder::flush(std::deque<DecodedFrame> &frames)
    {
        return m_impl->flush(frames);
    }

    void FfmpegDecoder::reset()
    {
        m_impl->reset();
    }

    void FfmpegDecoder::setKeepYuv(bool keep)
    {
        m_impl->setKeepYuv(keep);
    }

    void toRgb32Macroblocks(const DecodedFrame &frame, std::vector<uint8_t> &out)
    {
        // libmpeg's RGB32 picture: macroblock columns, each a strip of 16-pixel rows the height of the picture.
        const uint32_t width = static_cast<uint32_t>(std::max(frame.width, 0));
        const uint32_t height = static_cast<uint32_t>(std::max(frame.height, 0));
        const uint32_t outWidth = (width + 15u) & ~15u;
        const uint32_t outHeight = (height + 15u) & ~15u;
        out.assign(static_cast<size_t>(outWidth) * outHeight * 4u, 0u);
        for (uint32_t mbx = 0u; mbx < outWidth / 16u; ++mbx)
        {
            const size_t stripOffset = static_cast<size_t>(mbx) * outHeight * 16u * 4u;
            for (uint32_t y = 0u; y < outHeight; ++y)
            {
                uint8_t *dst = out.data() + stripOffset + static_cast<size_t>(y) * 16u * 4u;
                for (uint32_t x = 0u; x < 16u; ++x)
                {
                    const uint32_t srcX = mbx * 16u + x;
                    if (srcX < width && y < height && !frame.rgba.empty())
                    {
                        const uint8_t *src = frame.rgba.data() + (static_cast<size_t>(y) * width + srcX) * 4u;
                        dst[x * 4u + 0u] = src[0u];
                        dst[x * 4u + 1u] = src[1u];
                        dst[x * 4u + 2u] = src[2u];
                    }
                    dst[x * 4u + 3u] = 0x80u;
                }
            }
        }
    }

    void toRaw8Macroblocks(const DecodedFrame &frame, std::vector<uint8_t> &out)
    {
        // libmpeg's RAW8 picture: the same column order, 384 bytes per macroblock (Y 16x16, Cb 8x8, Cr 8x8).
        const uint32_t width = static_cast<uint32_t>(std::max(frame.width, 0));
        const uint32_t height = static_cast<uint32_t>(std::max(frame.height, 0));
        const uint32_t mbColumns = (width + 15u) / 16u;
        const uint32_t mbRows = (height + 15u) / 16u;
        const uint32_t chromaWidth = (width + 1u) / 2u;
        const uint32_t chromaHeight = (height + 1u) / 2u;
        out.assign(static_cast<size_t>(mbColumns) * mbRows * 384u, 0u);
        if (frame.y.empty() || frame.cb.empty() || frame.cr.empty())
            return;
        for (uint32_t mbx = 0u; mbx < mbColumns; ++mbx)
            for (uint32_t mby = 0u; mby < mbRows; ++mby)
            {
                uint8_t *mb = out.data() + (static_cast<size_t>(mbx) * mbRows + mby) * 384u;
                for (uint32_t y = 0u; y < 16u; ++y)
                    for (uint32_t x = 0u; x < 16u; ++x)
                    {
                        const uint32_t px = mbx * 16u + x, py = mby * 16u + y;
                        mb[y * 16u + x] = (px < width && py < height) ? frame.y[static_cast<size_t>(py) * width + px] : 0u;
                    }
                for (uint32_t y = 0u; y < 8u; ++y)
                    for (uint32_t x = 0u; x < 8u; ++x)
                    {
                        const uint32_t px = mbx * 8u + x, py = mby * 8u + y;
                        const bool inside = px < chromaWidth && py < chromaHeight;
                        const size_t index = static_cast<size_t>(py) * chromaWidth + px;
                        mb[256u + y * 8u + x] = inside ? frame.cb[index] : 0x80u;
                        mb[320u + y * 8u + x] = inside ? frame.cr[index] : 0x80u;
                    }
            }
    }
}
