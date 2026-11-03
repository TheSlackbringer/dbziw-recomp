// Authored regression (DBZIW 037): sceSifRegisterRpc(sd, sid, func, buf, cfunc, cbuf, qd) takes its fifth to
// seventh arguments from t0..t2, as the EE ABI passes them; the handler used to read them from the caller's stack.
// The stack words here hold other values, so a stack read shows. No game code or data.
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
constexpr uint32_t kServer = 0x20000u, kQueue = 0x21000u, kStack = 0x30000u;
constexpr uint32_t kSid = 0x90000200u, kFunc = 0x123450u, kBuf = 0x24000u;
constexpr uint32_t kCfunc = 0x123460u, kCbuf = 0x25000u;

void reg(R5900Context &c, unsigned n, uint32_t v) { c.r[n] = _mm_cvtsi32_si128(static_cast<int>(v)); }
uint32_t word(const std::vector<uint8_t> &ram, uint32_t at)
{
    uint32_t v = 0;
    std::memcpy(&v, ram.data() + at, sizeof(v));
    return v;
}
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
        std::vector<uint8_t> ram(0x2000000u, 0);
        for (uint32_t off = 0x10u; off <= 0x18u; off += 4u) // stack words that must not be used
        {
            const uint32_t junk = 0xDEAD0000u | off;
            std::memcpy(ram.data() + kStack + off, &junk, sizeof(junk));
        }
        R5900Context ctx{};
        reg(ctx, 4, kServer);
        reg(ctx, 5, kSid);
        reg(ctx, 6, kFunc);
        reg(ctx, 7, kBuf);
        reg(ctx, 8, kCfunc);
        reg(ctx, 9, kCbuf);
        reg(ctx, 10, kQueue);
        reg(ctx, 29, kStack);
        ps2_syscalls::SifRegisterRpc(ram.data(), &ctx, &runtime);

        require(word(ram, kServer + 0) == kSid, "sid");
        require(word(ram, kServer + 4) == kFunc, "func");
        require(word(ram, kServer + 8) == kBuf, "buf");
        require(word(ram, kServer + 16) == kCfunc, "cfunc comes from t0");
        require(word(ram, kServer + 20) == kCbuf, "cbuf comes from t1");
        require(word(ram, kServer + 64) == kQueue, "the queue comes from t2");
        require(word(ram, kQueue + 8) == kServer, "the server is linked into the queue from t2");
        std::cout << "sceSifRegisterRpc argument tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
