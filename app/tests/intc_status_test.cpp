// Authored regression (DBZIW 036): the EE interrupt controller's INTC_STAT (0x1000F000) and INTC_MASK (0x1000F010)
// as guest-visible registers. STAT latches every raised cause whatever the mask, a guest write of 1 clears it, and
// the kernel acknowledges a cause when it takes the interrupt; a cause no handler takes stays set, so code that
// polls STAT (libgraph's sceGsSyncV without a callback) sees VBLANK start. MASK reads the scheduler's enabled
// causes and a guest write of 1 toggles them there. No game code or data.
#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr uint32_t kHandlerPc = 0x1000u;
constexpr uint32_t kStat = 0x1000F000u;
constexpr uint32_t kMask = 0x1000F010u;
constexpr uint32_t kVblankStart = 2u;
constexpr uint32_t kVblankEnd = 3u;

void handler(uint8_t *, R5900Context *ctx, PS2Runtime *) { ctx->pc = 0u; }

void require(bool ok, const char *what)
{
    if (!ok)
    {
        std::cerr << "FAILED: " << what << '\n';
        throw std::runtime_error(what);
    }
}
}

extern const uint32_t g_ps2RecompiledFunctionTableBase = kHandlerPc;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = kHandlerPc + 4u;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1u;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1] = {handler};

int main()
{
    try
    {
        PS2Runtime runtime;
        std::vector<uint8_t> ram(0x2000000u);
        R5900Context main{};
        main.pc = kHandlerPc;
        auto &kernel = runtime.eeScheduler();
        kernel.reset(ram.data(), main);
        auto &memory = runtime.memory();

        require(memory.read32(kStat) == 0u, "STAT starts clear");
        require(memory.read32(kMask) == 0xFFFFFFFFu, "MASK reads the scheduler's mask (all causes enabled)");

        // No handler for VBLANK start: the cause stays latched until the guest clears it.
        kernel.dispatchIrq(false, kVblankStart);
        require(memory.read32(kStat) == (1u << kVblankStart), "VBLANK start latched");
        memory.write32(kStat, 1u << kVblankEnd);
        require(memory.read32(kStat) == (1u << kVblankStart), "writing another bit leaves it set");
        memory.write32(kStat, 1u << kVblankStart);
        require(memory.read32(kStat) == 0u, "write 1 clears");

        // A masked cause latches too.
        kernel.setIrqCauseEnabled(false, kVblankEnd, false);
        require((memory.read32(kMask) & (1u << kVblankEnd)) == 0u, "DisableIntc shows in MASK");
        kernel.dispatchIrq(false, kVblankEnd);
        require(memory.read32(kStat) == (1u << kVblankEnd), "masked cause latched");

        // A guest MASK write toggles the scheduler's mask: re-enable VBLANK end by writing its bit.
        memory.write32(kMask, 1u << kVblankEnd);
        require((memory.read32(kMask) & (1u << kVblankEnd)) != 0u, "MASK write toggles the bit back on");
        memory.write32(kStat, 0xFFFFFFFFu);

        // With a handler, the kernel takes the interrupt and acknowledges the cause.
        require(kernel.addIrqHandler(false, kVblankEnd, kHandlerPc, true, 0u, 0u, 0u) > 0, "handler added");
        kernel.dispatchIrq(false, kVblankEnd);
        require((memory.read32(kStat) & (1u << kVblankEnd)) == 0u, "taken cause acknowledged");
        kernel.dispatchIrq(false, kVblankStart);
        require(memory.read32(kStat) == (1u << kVblankStart), "a cause without a handler is still latched");

        // DMAC causes do not touch INTC_STAT.
        memory.write32(kStat, 0xFFFFFFFFu);
        kernel.dispatchIrq(true, kVblankStart);
        require(memory.read32(kStat) == 0u, "DMAC dispatch leaves INTC_STAT alone");

        std::cout << "INTC_STAT/INTC_MASK tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
