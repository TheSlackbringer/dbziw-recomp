// Authored regression (DBZIW 033): the GS CSR register as the guest writes it. A write clears only the interrupt
// flags SIGNAL/FINISH/HSINT/VSINT/EDWINT (bits 0..4, write one to clear); FLUSH and RESET are one-shot commands
// that never read back; NFIELD/FIELD/FIFO and the revision/ID byte pair are read-only. libgraph's own
// sceGsResetGraph writes RESET (0x200) and then reads the revision. No game code or data.
#include "ps2_runtime.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
constexpr uint32_t kCsr = 0x12001000u;

void require(bool ok, const char *what)
{
    if (!ok)
    {
        std::cerr << "FAILED: " << what << '\n';
        throw std::runtime_error(what);
    }
}
}

int main()
{
    try
    {
        PS2Runtime runtime;
        auto &memory = runtime.memory();
        auto &csr = memory.gs().csr;
        const uint64_t revisionId = 0x551B0000ull;
        csr.store(revisionId | 0x2000ull | 0x3ull); // revision/ID, FIELD, SIGNAL and FINISH pending

        memory.write64(kCsr, 0x200u); // RESET, as sceGsResetGraph(0, ...) does
        require((memory.read64(kCsr) & 0x200u) == 0u, "RESET does not read back");
        require((memory.read64(kCsr) & 0xFFFF0000u) == revisionId, "revision/ID survive a write");
        require((memory.read64(kCsr) & 0x2000u) != 0u, "FIELD survives a write");
        require((memory.read64(kCsr) & 0x3u) == 0x3u, "writing 0 to the flags leaves them pending");

        memory.write64(kCsr, 0x2u); // clear FINISH only
        require((memory.read64(kCsr) & 0x3u) == 0x1u, "write one clears FINISH");
        memory.write32(kCsr, 0x1u); // a 32-bit write clears SIGNAL
        require((memory.read64(kCsr) & 0x3u) == 0u, "write one clears SIGNAL");

        memory.write32(kCsr + 4u, 0xFFFFFFFFu); // the upper word is read-only
        require((memory.read64(kCsr) >> 32) == 0u, "upper word unchanged");
        memory.write64(kCsr, 0x100u); // FLUSH
        require(memory.read64(kCsr) == (revisionId | 0x2000ull), "FLUSH does not read back; nothing else changed");

        std::cout << "GS CSR tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
