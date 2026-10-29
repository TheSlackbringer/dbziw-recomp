// DBZIW (PAL SLES_553.47) Sofdec movies: Sony's libmpeg replaced by a native decoder, without the IPU (issue 020).
//
// Sofdec (CRI MWSFD 4.46) calls seven libmpeg entry points. They are the same Sony library as DW5's (DW5 address +
// 0x176580), so the public sceMpeg struct and callback rules are DW5's; the input is not: Sofdec feeds the IPU
// itself through a DMA4 chain and saves/restores the IPU position around each picture request (see
// dbziw_movie_input.h). Decoder state is keyed by mp->sys (+0x40, the work area), because Sofdec copies the 0x48-byte
// sceMpeg struct to and from a spare global (0x4c4d70) to reuse a decoder without sceMpegCreate.
//
// Callbacks (slot sys + 0xc + type * 0xc: func, data, $gp) run as nested guest calls on the calling thread, since
// they take Sofdec's semaphore: no data (1) when DMA4 has stopped, then stop DMA (2) and restart DMA (3) around each
// RGB32 picture, as libmpeg's output path FUN_00291978 does. The replacement returns into itself after each one.
#include "dbziw_movie_input.h"
#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ee_scheduler.h"
#include "runtime/ps2_memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uint32_t kMpegCreate = 0x0028F9F8u;
    constexpr uint32_t kMpegInit = 0x002905A0u;
    constexpr uint32_t kMpegGetPicture = 0x00290698u;
    constexpr uint32_t kMpegGetPictureRaw8 = 0x002906E0u;
    constexpr uint32_t kMpegSetEnd = 0x00290728u; // sets the end flag (sys + 0x878); Sofdec calls it after 11 no-data calls
    constexpr uint32_t kMpegReset = 0x00290738u;
    constexpr uint32_t kMpegAddCallback = 0x00290810u;

    constexpr uint32_t kSysEnd = 0x878u;
    constexpr uint32_t kSysMp = 0x868u;
    constexpr uint32_t kMinimumWork = 0x19A0u;
    constexpr uint32_t kCallbackNoData = 1u, kCallbackStopDma = 2u, kCallbackRestartDma = 3u;

    enum class Phase
    {
        Decoding,
        AfterStopDma,    // picture written; restart DMA comes next
        AfterRestartDma, // picture request finished
    };

    struct Stream
    {
        dbziw::movies::VideoInput input;
        Phase phase = Phase::Decoding;
        uint32_t frameCount = 0u;
        uint64_t noDataCalls = 0u;
    };

    std::unordered_map<uint32_t, std::unique_ptr<Stream>> g_streams; // by mp->sys
    Stream *g_active = nullptr; // the stream whose IPU position the IPU registers report

    bool traceEnabled()
    {
        static const bool enabled = std::getenv("PS2X_SOFDEC_TRACE") != nullptr; // DBZIW-DEBUG
        return enabled;
    }

    uint32_t read32(uint8_t *rdram, uint32_t address)
    {
        uint32_t value = 0u;
        if (const uint8_t *p = getMemPtr(rdram, address))
            std::memcpy(&value, p, sizeof(value));
        return value;
    }

    void write32(uint8_t *rdram, uint32_t address, uint32_t value)
    {
        if (uint8_t *p = getMemPtr(rdram, address))
            std::memcpy(p, &value, sizeof(value));
    }

    void write64(uint8_t *rdram, uint32_t address, uint64_t value)
    {
        if (uint8_t *p = getMemPtr(rdram, address))
            std::memcpy(p, &value, sizeof(value));
    }

    struct IpuHook : PS2Memory::IpuInputHook
    {
        uint32_t ipuBp() const override { return g_active ? g_active->input.ipuBp() : 0u; }
        void ipuBclr(uint32_t bp) override
        {
            if (g_active)
                g_active->input.bclr(bp);
        }
    } g_ipuHook;

    Stream &streamFor(uint32_t sys)
    {
        std::unique_ptr<Stream> &stream = g_streams[sys];
        if (!stream)
            stream = std::make_unique<Stream>();
        return *stream;
    }

    void finish(R5900Context *ctx, int32_t result)
    {
        setReturnS32(ctx, result);
        ctx->pc = getRegU32(ctx, 31);
    }

    // libmpeg's FUN_0028fd30: stop DMA3/DMA4, clear their counts, reset the IPU.
    void stopIpuDma(PS2Memory &memory)
    {
        memory.write32(0x1000B000u, 0u);
        memory.write32(0x1000B400u, memory.read32(0x1000B400u) & ~0x100u);
        memory.write32(0x1000B020u, 0u);
        memory.write32(0x1000B420u, 0u);
        memory.write32(0x10002010u, 0x40000000u);
    }

    // Runs callback `type` as a nested guest call; the caller then re-enters `reentry`. False if none is registered.
    bool invokeCallback(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, uint32_t mp, uint32_t sys, uint32_t type,
                        uint32_t reentry)
    {
        const uint32_t slot = sys + 0xCu + type * 0xCu;
        const uint32_t function = read32(rdram, slot);
        if (function == 0u || !runtime->hasFunction(function))
            return false;
        EeScheduler &scheduler = runtime->eeScheduler();
        scheduler.bindMainContextForSyscall(*ctx, rdram);
        // libmpeg passes the type in a stack record: {type, 0...}; keep it below the nested call's stack.
        const uint32_t record = scheduler.invocationStackTop() - 0x40u;
        for (uint32_t i = 0u; i < 0x40u; i += 4u)
            write32(rdram, record + i, i == 0u ? type : 0u);
        GuestInvocation invocation{};
        invocation.kind = GuestInvocationKind::HleCall;
        invocation.context = *ctx;
        invocation.context.pc = function;
        SET_GPR_U32(&invocation.context, 4, mp);
        SET_GPR_U32(&invocation.context, 5, record);
        SET_GPR_U32(&invocation.context, 6, read32(rdram, slot + 4u));
        SET_GPR_U32(&invocation.context, 7, 0u);
        SET_GPR_U32(&invocation.context, 28, read32(rdram, slot + 8u));
        SET_GPR_U32(&invocation.context, 29, record);
        SET_GPR_U32(&invocation.context, 31, 0u);
        ctx->pc = reentry;
        if (traceEnabled())
            std::fprintf(stderr, "[sofdec] callback type=%u func=0x%x mp=0x%x\n", type, function, mp);
        scheduler.invokeCurrent(std::move(invocation));
    }

    void mpegInit(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        stopIpuDma(runtime->memory());
        runtime->memory().setIpuInputHook(&g_ipuHook);
        finish(ctx, 1);
    }

    void mpegCreate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t mp = getRegU32(ctx, 4);
        const uint32_t work = getRegU32(ctx, 5);
        const int32_t size = static_cast<int32_t>(getRegU32(ctx, 6));
        for (int32_t i = 0; i < size; ++i)
            if (uint8_t *p = getMemPtr(rdram, work + static_cast<uint32_t>(i)))
                *p = 0u;
        const uint32_t sys = (work + 3u) & ~3u;
        if (size - static_cast<int32_t>(sys - work) < static_cast<int32_t>(kMinimumWork))
        {
            std::fprintf(stderr, "[sofdec] sceMpegCreate: work area of %d bytes is too small\n", size);
            finish(ctx, 0);
            return;
        }
        // Sony's sceMpeg: width, height, frameCount, pts, dts, flags, pts2nd, dts2nd, flags2nd, sys.
        write32(rdram, mp + 0x00u, 0u);
        write32(rdram, mp + 0x04u, 0u);
        write32(rdram, mp + 0x08u, 0u);
        write64(rdram, mp + 0x10u, ~0ull);
        write64(rdram, mp + 0x18u, ~0ull);
        write64(rdram, mp + 0x20u, 0u);
        write64(rdram, mp + 0x28u, ~0ull);
        write64(rdram, mp + 0x30u, ~0ull);
        write64(rdram, mp + 0x38u, 0u);
        write32(rdram, mp + 0x40u, sys);
        write32(rdram, sys + kSysMp, mp);
        write32(rdram, sys + kSysEnd, 0u);
        Stream &stream = streamFor(sys);
        stream.input.reset();
        stream.phase = Phase::Decoding;
        stream.frameCount = 0u;
        g_active = &stream;
        if (traceEnabled())
            std::fprintf(stderr, "[sofdec] create mp=0x%x sys=0x%x work=%d\n", mp, sys, size);
        finish(ctx, 1);
    }

    void mpegReset(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t mp = getRegU32(ctx, 4);
        const uint32_t sys = read32(rdram, mp + 0x40u);
        write32(rdram, sys + kSysEnd, 0u);
        write32(rdram, mp + 0x08u, 0u);
        stopIpuDma(runtime->memory());
        PS2Memory &memory = runtime->memory();
        memory.write32(0x10002010u, (memory.read32(0x10002010u) & ~0x00800000u) | 0x00800000u);
        Stream &stream = streamFor(sys);
        stream.input.reset();
        stream.phase = Phase::Decoding;
        stream.frameCount = 0u;
        g_active = &stream;
        finish(ctx, 1);
    }

    void mpegAddCallback(uint8_t *rdram, R5900Context *ctx, PS2Runtime *)
    {
        const uint32_t mp = getRegU32(ctx, 4);
        const uint32_t type = getRegU32(ctx, 5);
        const uint32_t sys = read32(rdram, mp + 0x40u);
        g_active = &streamFor(sys); // Sofdec restores the IPU position right after registering its callbacks
        const uint32_t slot = sys + 0xCu + type * 0xCu;
        const uint32_t previous = read32(rdram, slot);
        write32(rdram, slot, getRegU32(ctx, 6));
        write32(rdram, slot + 4u, getRegU32(ctx, 7));
        write32(rdram, slot + 8u, getRegU32(ctx, 28));
        finish(ctx, static_cast<int32_t>(previous));
    }

    void mpegSetEnd(uint8_t *rdram, R5900Context *ctx, PS2Runtime *)
    {
        write32(rdram, read32(rdram, getRegU32(ctx, 4) + 0x40u) + kSysEnd, 1u);
        ctx->pc = getRegU32(ctx, 31);
    }

    void writePicture(uint8_t *rdram, uint32_t mp, uint32_t buffer, bool rgb32, Stream &stream,
                      const ps2x::mpeg::DecodedFrame &frame)
    {
        std::vector<uint8_t> out;
        if (rgb32)
            ps2x::mpeg::toRgb32Macroblocks(frame, out);
        else
            ps2x::mpeg::toRaw8Macroblocks(frame, out);
        for (size_t offset = 0u; offset < out.size(); offset += 16u)
            if (uint8_t *p = getMemPtr(rdram, buffer + static_cast<uint32_t>(offset)))
                std::memcpy(p, out.data() + offset, 16u);
        write32(rdram, mp + 0x00u, static_cast<uint32_t>(frame.width));
        write32(rdram, mp + 0x04u, static_cast<uint32_t>(frame.height));
        write32(rdram, mp + 0x08u, ++stream.frameCount);
        write64(rdram, mp + 0x10u, ~0ull); // no PES timestamps: Sofdec feeds the bare video stream
        write64(rdram, mp + 0x18u, ~0ull);
    }

    void getPicture(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, bool rgb32)
    {
        const uint32_t entry = rgb32 ? kMpegGetPicture : kMpegGetPictureRaw8;
        const uint32_t mp = getRegU32(ctx, 4);
        const uint32_t buffer = getRegU32(ctx, 5);
        const uint32_t macroblocks = getRegU32(ctx, 6);
        const uint32_t sys = read32(rdram, mp + 0x40u);
        Stream &stream = streamFor(sys);
        g_active = &stream;
        PS2Memory &memory = runtime->memory();

        if (stream.phase == Phase::AfterStopDma)
        {
            stream.phase = Phase::AfterRestartDma;
            invokeCallback(rdram, ctx, runtime, mp, sys, kCallbackRestartDma, entry);
        }
        if (stream.phase == Phase::AfterRestartDma)
        {
            stream.phase = Phase::Decoding;
            finish(ctx, 1);
            return;
        }

        for (;;)
        {
            if (read32(rdram, sys + kSysEnd) != 0u)
            {
                finish(ctx, -1); // libmpeg returns -1 once the end flag is set
                return;
            }
            ps2x::mpeg::DecodedFrame frame;
            const auto result = stream.input.next(frame);
            if (result == dbziw::movies::VideoInput::Result::Ended)
            {
                finish(ctx, -1);
                return;
            }
            if (result == dbziw::movies::VideoInput::Result::NeedData)
            {
                if (dbziw::movies::pullDma4Quadword(memory, stream.input))
                    continue;
                ++stream.noDataCalls;
                if (!invokeCallback(rdram, ctx, runtime, mp, sys, kCallbackNoData, entry))
                {
                    finish(ctx, -1);
                    return;
                }
            }
            const uint32_t needed = static_cast<uint32_t>(((frame.width + 15) / 16) * ((frame.height + 15) / 16));
            if (needed > macroblocks)
            {
                std::fprintf(stderr, "[sofdec] picture %dx%d needs %u macroblocks, buffer has %u\n", frame.width,
                             frame.height, needed, macroblocks);
                finish(ctx, -1);
                return;
            }
            writePicture(rdram, mp, buffer, rgb32, stream, frame);
            if (traceEnabled())
                std::fprintf(stderr, "[sofdec] picture %u %dx%d type=%d %s held=%u noData=%llu tick=%llu\n",
                             stream.frameCount, frame.width, frame.height, frame.pictType, rgb32 ? "rgb32" : "raw8",
                             stream.input.unusedQuadwords(), static_cast<unsigned long long>(stream.noDataCalls),
                             static_cast<unsigned long long>(runtime->memory().gs().vsyncTick.load()));
            if (rgb32)
            {
                stream.phase = Phase::AfterStopDma;
                invokeCallback(rdram, ctx, runtime, mp, sys, kCallbackStopDma, entry);
                stream.phase = Phase::AfterRestartDma;
                invokeCallback(rdram, ctx, runtime, mp, sys, kCallbackRestartDma, entry);
                stream.phase = Phase::Decoding;
            }
            finish(ctx, 1);
            return;
        }
    }

    void mpegGetPicture(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { getPicture(rdram, ctx, runtime, true); }
    void mpegGetPictureRaw8(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { getPicture(rdram, ctx, runtime, false); }

    void applyMovies(PS2Runtime &runtime)
    {
        struct Entry
        {
            uint32_t address;
            uint32_t firstWord;
            PS2Runtime::RecompiledFunction function;
        };
        const Entry entries[] = {
            {kMpegInit, 0x27BDFFF0u, &mpegInit},
            {kMpegCreate, 0x27BDFFB0u, &mpegCreate},
            {kMpegReset, 0x27BDFFE0u, &mpegReset},
            {kMpegAddCallback, 0x2402000Cu, &mpegAddCallback},
            {kMpegGetPicture, 0x27BDFFF0u, &mpegGetPicture},
            {kMpegGetPictureRaw8, 0x27BDFFF0u, &mpegGetPictureRaw8},
            {kMpegSetEnd, 0x8C830040u, &mpegSetEnd},
        };
        uint8_t *rdram = runtime.memory().getRDRAM();
        for (const Entry &entry : entries)
        {
            if (read32(rdram, entry.address) != entry.firstWord)
            {
                std::fprintf(stderr, "[sofdec] libmpeg not where expected (0x%x holds 0x%08x); movies stay unpatched\n",
                             entry.address, read32(rdram, entry.address));
                return;
            }
        }
        for (const Entry &entry : entries)
            runtime.replaceFunction(entry.address, entry.function);
        runtime.memory().setIpuInputHook(&g_ipuHook);
    }
}

PS2_REGISTER_GAME_OVERRIDE("DBZIW native libmpeg for Sofdec movies", "SLES_553.47", 0u, 0u, &applyMovies)
