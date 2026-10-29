// DBZIW Sofdec movie video input, without the IPU (issue 020). See dbziw_movie_input.h.
#include "dbziw_movie_input.h"

#include "runtime/ps2_memory.h"

#include <algorithm>
#include <cstring>

namespace dbziw::movies
{
    namespace
    {
        constexpr uint8_t kPictureStart = 0x00u;
        constexpr uint8_t kSequenceHeader = 0xB3u;
        constexpr uint8_t kSequenceEnd = 0xB7u;
        constexpr uint8_t kGroupStart = 0xB8u;

        // Next start code prefix 00 00 01 at or after `from`; the code byte must be present too.
        bool nextStartCode(const std::vector<uint8_t> &data, size_t from, size_t &at)
        {
            for (size_t i = from; i + 3u < data.size(); ++i)
            {
                if (data[i] == 0u && data[i + 1u] == 0u && data[i + 2u] == 1u)
                {
                    at = i;
                    return true;
                }
            }
            return false;
        }
    }

    VideoInput::VideoInput()
    {
        m_decoder.setKeepYuv(true);
    }

    void VideoInput::reset()
    {
        m_decoder.reset();
        m_decoder.setKeepYuv(true);
        m_frames.clear();
        m_fifo.clear();
        m_used = 0u;
        m_skipBytes = 0u;
        m_sequenceEnded = false;
        m_picturesDecoded = 0u;
        restartScan();
    }

    void VideoInput::restartScan()
    {
        m_scanFrom = m_used;
        m_scanStarted = false;
        m_scanPicture = false;
    }

    void VideoInput::push(const uint8_t quadword[16])
    {
        const bool restart = m_fifo.empty();
        m_fifo.insert(m_fifo.end(), quadword, quadword + 16);
        if (restart && m_skipBytes != 0u)
        {
            m_used = m_skipBytes;
            m_skipBytes = 0u;
            restartScan();
        }
    }

    void VideoInput::bclr(uint32_t bp)
    {
        // Our input is whole bytes: Sofdec only restores positions this class reported, which are byte aligned.
        m_fifo.clear();
        m_used = 0u;
        m_skipBytes = (bp & 0x7Fu) / 8u;
        restartScan();
    }

    uint32_t VideoInput::unusedQuadwords() const
    {
        return static_cast<uint32_t>((m_fifo.size() - (m_used & ~size_t{15u})) / 16u);
    }

    uint32_t VideoInput::ipuBp() const
    {
        if (m_fifo.empty())
            return m_skipBytes * 8u;
        const uint32_t held = unusedQuadwords();
        const uint32_t fp = std::min<uint32_t>(held, 2u);
        const uint32_t ifc = std::min<uint32_t>(held - fp, 8u);
        const uint32_t bp = static_cast<uint32_t>(m_used & 15u) * 8u;
        return bp | (ifc << 8u) | (fp << 16u);
    }

    bool VideoInput::findPicture(size_t &end, bool &sequenceEnd)
    {
        // One packet: any headers, one picture header and its slices, up to the next picture, sequence header,
        // group of pictures or sequence end. The next start code must already be here, as it is for the IPU's own
        // look-ahead before libmpeg finishes a picture. The search resumes where the last one stopped, so each byte
        // is looked at once however the input arrives.
        size_t at = 0u;
        while (nextStartCode(m_fifo, m_scanFrom, at))
        {
            const uint8_t code = m_fifo[at + 3u];
            m_scanFrom = at + 4u;
            if (!m_scanStarted && code == kSequenceEnd)
            {
                end = at + 4u;
                sequenceEnd = true;
                return true;
            }
            m_scanStarted = true;
            const bool boundary = code == kPictureStart || code == kSequenceHeader || code == kGroupStart || code == kSequenceEnd;
            if (m_scanPicture && boundary)
            {
                end = at;
                sequenceEnd = false;
                return true;
            }
            m_scanPicture = m_scanPicture || code == kPictureStart;
        }
        // Every position before the last three bytes has been searched; a prefix may still start in those.
        if (m_fifo.size() >= 3u)
            m_scanFrom = std::max(m_scanFrom, m_fifo.size() - 3u);
        return false;
    }

    void VideoInput::dropUsedQuadwords()
    {
        const size_t drop = m_used & ~size_t{15u};
        if (drop == 0u)
            return;
        m_fifo.erase(m_fifo.begin(), m_fifo.begin() + static_cast<ptrdiff_t>(drop));
        m_used -= drop;
        m_scanFrom -= drop;
    }

    VideoInput::Result VideoInput::next(ps2x::mpeg::DecodedFrame &frame)
    {
        for (;;)
        {
            if (!m_frames.empty())
            {
                frame = std::move(m_frames.front());
                m_frames.pop_front();
                return Result::Picture;
            }
            if (m_sequenceEnded)
                return Result::Ended;

            size_t end = 0u;
            bool sequenceEnd = false;
            if (!findPicture(end, sequenceEnd))
                return Result::NeedData;
            if (sequenceEnd)
            {
                m_decoder.flush(m_frames);
                m_sequenceEnded = true;
            }
            else
            {
                m_decoder.decodePacket(m_fifo.data() + m_used, end - m_used, m_frames);
                ++m_picturesDecoded;
            }
            m_used = end;
            restartScan();
            dropUsedQuadwords();
        }
    }
}

namespace dbziw::movies
{
    bool pullDma4Quadword(PS2Memory &memory, VideoInput &input)
    {
        constexpr uint32_t kChcr = 0x1000B400u, kMadr = 0x1000B410u, kQwc = 0x1000B420u;
        memory.creditIpuInputBytes(0u); // fetches the next tag once the current one is used up
        if ((memory.read32(kChcr) & 0x100u) == 0u || memory.read32(kQwc) == 0u)
            return false;
        const uint32_t madr = memory.read32(kMadr);
        const uint8_t *source = (madr & 0x80000000u) != 0u
                                    ? memory.getScratchpad() + (madr & (PS2_SCRATCHPAD_SIZE - 16u))
                                    : memory.getRDRAM() + (madr & (PS2_RAM_SIZE - 16u));
        input.push(source);
        memory.creditIpuInputBytes(16u);
        return true;
    }
}
