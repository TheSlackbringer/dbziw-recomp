// Authored regression (DBZIW 037): Deci2Call sets its result in every build. Its body used to be compiled only with
// _DEBUG or RUNTIME_DECI2CALL, so in Release v0 kept the caller's value and a guest console driver took a failed
// sceDeci2Open for a socket. With no debugger attached, the open fails (-1), the way libkernel's read/write expect.
// Built against the Release runtime, like the game. No game code or data.
#include "ps2_runtime.h"
#include "ps2_syscalls.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
constexpr uint32_t kArgs = 0x20000u, kText = 0x21000u;
constexpr uint32_t kStale = 0x2049d710u; // what v0 held before the call in the game's open path

void reg(R5900Context &c, unsigned n, uint32_t v) { c.r[n] = _mm_cvtsi32_si128(static_cast<int>(v)); }
int32_t result(const R5900Context &c) { return _mm_cvtsi128_si32(c.r[2]); }
void require(bool ok, const char *what)
{
    if (!ok)
        throw std::runtime_error(what);
}

int32_t call(std::vector<uint8_t> &ram, PS2Runtime &runtime, int32_t code, std::vector<uint32_t> args)
{
    std::memcpy(ram.data() + kArgs, args.data(), args.size() * sizeof(uint32_t));
    R5900Context c{};
    reg(c, 2, kStale);
    reg(c, 4, static_cast<uint32_t>(code));
    reg(c, 5, kArgs);
    ps2_syscalls::Deci2Call(ram.data(), &c, &runtime);
    return result(c);
}
}

int main()
{
    try
    {
        PS2Runtime runtime;
        std::vector<uint8_t> ram(0x2000000u, 0);
        require(call(ram, runtime, 1, {0x210u, 0x49d850u, 0x103360u, 0u}) == -1,
                "sceDeci2Open fails: no debugger attached");
        require(call(ram, runtime, 3, {1u, 0x48u, 0u, 0u}) == 0, "sceDeci2ReqSend answers 0");
        require(call(ram, runtime, 4, {1u, 0u, 0u, 0u}) == 0, "sceDeci2Poll answers 0");
        std::memcpy(ram.data() + kText, "hello\n", 7);
        require(call(ram, runtime, 0x10, {kText, 0u, 0u, 0u}) == 6, "kputs returns the length");
        require(call(ram, runtime, 0x55, {0u, 0u, 0u, 0u}) == 0, "an unknown code answers 0");
        std::cout << "Deci2Call result tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
