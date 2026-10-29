// DBZIW (PAL SLES_553.47) Sofdec movie video input, without the IPU (issue 020).
//
// Sofdec feeds its MPEG video stream to the IPU through a DMA4 chain and, around each picture request, saves and
// restores the IPU's input position itself: it reads IPU_BP (bit pointer, input FIFO count IFC, bit buffer count FP),
// moves D4_MADR back by 16 * (IFC + FP) and writes IPU_CMD = BCLR | bp. This class takes the same quadwords the IPU
// would take, hands whole pictures to the shared FFmpeg decoder, and reports the position the IPU would report.
#pragma once

#include "runtime/ps2_mpeg_video.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

class PS2Memory;

namespace dbziw::movies
{
    class VideoInput
    {
    public:
        enum class Result
        {
            Picture, // `frame` holds the next picture in display order
            NeedData, // push more quadwords
            Ended,    // the sequence end code was decoded and every picture has been handed out
        };

        VideoInput();

        void reset();
        void push(const uint8_t quadword[16]);
        // IPU BCLR: drop unused input; the next quadword starts `bp` bits in.
        void bclr(uint32_t bp);
        // IPU_BP as the IPU would report it now: BP | IFC << 8 | FP << 16.
        uint32_t ipuBp() const;
        Result next(ps2x::mpeg::DecodedFrame &frame);

        // Quadwords held but not yet used; the IPU holds at most 10 (8 in the FIFO, 2 in the bit buffer).
        uint32_t unusedQuadwords() const;
        uint64_t picturesDecoded() const { return m_picturesDecoded; }

    private:
        bool findPicture(size_t &end, bool &sequenceEnd);
        void restartScan();
        void dropUsedQuadwords();

        ps2x::mpeg::FfmpegDecoder m_decoder;
        std::deque<ps2x::mpeg::DecodedFrame> m_frames;
        std::vector<uint8_t> m_fifo; // whole quadwords, first one aligned to the DMA stream
        size_t m_used = 0u;          // bytes of m_fifo already given to the decoder
        uint32_t m_skipBytes = 0u;   // from BCLR, applied to the next quadword
        bool m_sequenceEnded = false;
        uint64_t m_picturesDecoded = 0u;
        size_t m_scanFrom = 0u;      // findPicture: next byte to search for a start code
        bool m_scanStarted = false;  // findPicture: a start code of this packet has been seen
        bool m_scanPicture = false;  // findPicture: this packet's picture header has been seen
    };

    // Takes one quadword from DMA4 into `input`, as the IPU takes it, and advances the channel through the runtime's
    // IPU-input sink (MADR/QWC/TADR, next tag, completion). False when the channel is stopped or has finished.
    bool pullDma4Quadword(PS2Memory &memory, VideoInput &input);
}
