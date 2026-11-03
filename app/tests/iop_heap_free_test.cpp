// Authored regression (DBZIW 037): sceSifFreeIopHeap releases a block from the same allocator sceSifAllocIopHeap
// takes it from, so a freed block is reused. With only the allocation bound, the game's own free does nothing (its
// heap RPC client is never bound) and every freed block leaks. No game code or data.
#include "ps2_runtime.h"
#include "ps2_stubs.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
void reg(R5900Context &c, unsigned n, uint32_t v) { c.r[n] = _mm_cvtsi32_si128(static_cast<int>(v)); }
uint32_t result(const R5900Context &c) { return static_cast<uint32_t>(_mm_cvtsi128_si32(c.r[2])); }
void require(bool ok, const char *what)
{
    if (!ok)
        throw std::runtime_error(what);
}

uint32_t alloc(std::vector<uint8_t> &ram, PS2Runtime &runtime, uint32_t size)
{
    R5900Context c{};
    reg(c, 4, size);
    ps2_stubs::sceSifAllocIopHeap(ram.data(), &c, &runtime);
    return result(c);
}

int32_t release(std::vector<uint8_t> &ram, PS2Runtime &runtime, uint32_t address)
{
    R5900Context c{};
    reg(c, 4, address);
    ps2_stubs::sceSifFreeIopHeap(ram.data(), &c, &runtime);
    return static_cast<int32_t>(result(c));
}
}

int main()
{
    try
    {
        PS2Runtime runtime;
        std::vector<uint8_t> ram(0x2000000u, 0);
        const uint32_t first = alloc(ram, runtime, 0x41040u);
        const uint32_t second = alloc(ram, runtime, 0x8d0u);
        require(first != 0u && second != 0u && first != second, "two allocations");
        require(release(ram, runtime, first) == 0, "freeing an allocated block returns 0");
        require(release(ram, runtime, first) == -1, "freeing it twice returns -1");
        require(release(ram, runtime, 0x1234560u) == -1, "freeing an address never allocated returns -1");
        require(alloc(ram, runtime, 0x41040u) == first, "the freed block is reused");
        require(release(ram, runtime, second) == 0, "the second block frees too");
        std::cout << "IOP heap free tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
