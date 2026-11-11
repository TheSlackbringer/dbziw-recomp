// DBZIW-TEST (039): a bind right after a module load finds the server the module's start code set up, with the IOP on
// its own thread. AMSOUND's start code only starts its RPC thread (the IOP kernel's StartThread makes it ready and does
// not switch to it); the server exists once the IOP has run that thread. The game binds server 0x11 right after the
// load and never retries (FUN_0013e650), so a bind that answers before the IOP had its turn hung 1 boot in ~15
// (2026-11-11). On the PS2 the IOP runs a ready thread as soon as the loader waits again, long before the EE's bind
// arrives.
//   1. Twenty fresh runtimes, IOP thread on: load a made-up module of that shape (start code: CreateThread +
//      StartThread; the thread: sceSifRegisterRpc, then SleepThread), then at once SifBindRpc (mode 0, waiting): the
//      client must get a server every time.
//      Also checked, so a FAIL means the race and not a broken module: the server does appear once the IOP has had
//      its turn (polled for up to 1 s after the bind).
//   2. A bind for a server that no module registers comes back without one, and promptly (it must not hang).
// No game data: the module is written here.
#include "ps2_runtime.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace ps2_syscalls
{
    void SifInitRpc(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime);
    void SifBindRpc(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime);
}

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
// Reach the private IOP-thread start and stop through explicit instantiation (as vu_minmax_test does): PS2Runtime
// starts the thread only inside run().
using IopThreadFn = void (PS2Runtime::*)();
IopThreadFn startIopThreadPtr();
IopThreadFn stopIopThreadPtr();
template<IopThreadFn Start, IopThreadFn Stop> struct IopThreadAccess
{
    friend IopThreadFn startIopThreadPtr() { return Start; }
    friend IopThreadFn stopIopThreadPtr() { return Stop; }
};
template struct IopThreadAccess<&PS2Runtime::startIopThread, &PS2Runtime::stopIopThread>;
// The IOP's own server table (what SifBindRpc asks), also private.
using CanBindFn = bool (PS2Runtime::*)(uint32_t) const noexcept;
CanBindFn canBindIopRpcPtr();
template<CanBindFn CanBind> struct CanBindAccess
{
    friend CanBindFn canBindIopRpcPtr() { return CanBind; }
};
template struct CanBindAccess<&PS2Runtime::canBindIopRpc>;

void require(bool ok, const char *why)
{
    if (!ok)
        throw std::runtime_error(why);
}

void setReg(R5900Context &ctx, unsigned index, uint32_t value)
{
    ctx.r[index] = _mm_setzero_si128();
    std::memcpy(&ctx.r[index], &value, sizeof(value));
}

void put32(std::vector<uint8_t> &bytes, uint32_t offset, uint32_t value) { std::memcpy(bytes.data() + offset, &value, 4); }
void put16(std::vector<uint8_t> &bytes, uint32_t offset, uint16_t value) { std::memcpy(bytes.data() + offset, &value, 2); }

constexpr uint32_t kLoad = 0x00010000u; // the module's load address (an absolute ELF, as the IOP tests' modules)
constexpr uint32_t kCreateThreadStub = kLoad + 0x114u, kStartThreadStub = kLoad + 0x11Cu, kSleepThreadStub = kLoad + 0x124u;
constexpr uint32_t kRegisterRpcStub = kLoad + 0x154u;
constexpr uint32_t jal(uint32_t target) { return 0x0C000000u | ((target >> 2u) & 0x03FFFFFFu); }

// An IRX laid out as: start code at 0x000, the RPC thread at 0x060, thbase imports at 0x100, sifcmd imports at 0x140,
// the thread descriptor at 0x180, server data 0x200, server function 0x300, buffer 0x400 (memsz 0x500).
std::vector<uint8_t> threadedRpcServerIrx(uint32_t sid)
{
    constexpr uint32_t codeOffset = 0x100u; // file offset of the loaded image
    std::vector<uint8_t> elf(codeOffset + 0x310u, 0u);
    const uint8_t ident[] = {0x7F, 'E', 'L', 'F', 1, 1, 1};
    std::memcpy(elf.data(), ident, sizeof(ident));
    put16(elf, 16, 2u);           // type: executable
    put16(elf, 18, 8u);           // machine: MIPS
    put32(elf, 20, 1u);           // version
    put32(elf, 24, kLoad);        // entry: the start code
    put32(elf, 28, 52u);          // program headers right after the ELF header
    put16(elf, 40, 52u);          // header size
    put16(elf, 42, 32u);          // program header size
    put16(elf, 44, 1u);           // one program header
    put32(elf, 52 + 0, 1u);       // PT_LOAD
    put32(elf, 52 + 4, codeOffset);
    put32(elf, 52 + 8, kLoad);
    put32(elf, 52 + 12, kLoad);
    put32(elf, 52 + 16, 0x310u);  // file size
    put32(elf, 52 + 20, 0x500u);  // memory size
    put32(elf, 52 + 24, 7u);      // RWX
    put32(elf, 52 + 28, 4u);

    const uint32_t start[] = {
        0x27BDFFE0u,            // addiu sp, sp, -0x20
        0xAFBF001Cu,            // sw    ra, 0x1c(sp)
        0x3C040001u,            // lui   a0, 1
        0x34840180u,            // ori   a0, a0, 0x180 (thread descriptor)
        jal(kCreateThreadStub), // jal   CreateThread
        0x00000000u,            // nop
        0x00402021u,            // addu  a0, v0, zero (thread id)
        jal(kStartThreadStub),  // jal   StartThread
        0x00002821u,            // addu  a1, zero, zero (delay slot)
        0x8FBF001Cu,            // lw    ra, 0x1c(sp)
        0x00001021u,            // addu  v0, zero, zero (RESIDENT_END)
        0x27BD0020u,            // addiu sp, sp, 0x20
        0x03E00008u,            // jr    ra
        0x00000000u,            // nop
    };
    const uint32_t thread[] = {
        0x27BDFFE0u,                             // addiu sp, sp, -0x20
        0xAFBF001Cu,                             // sw    ra, 0x1c(sp)
        0x3C040001u,                             // lui   a0, 1
        0x34840200u,                             // ori   a0, a0, 0x200 (server data)
        0x3C050000u | ((sid >> 16u) & 0xFFFFu),  // lui   a1, SID upper
        0x34A50000u | (sid & 0xFFFFu),           // ori   a1, a1, SID lower
        0x3C060001u,                             // lui   a2, 1
        0x34C60300u,                             // ori   a2, a2, 0x300 (server function)
        0x3C070001u,                             // lui   a3, 1
        0x34E70400u,                             // ori   a3, a3, 0x400 (server buffer)
        0xAFA00010u,                             // sw    zero, 0x10(sp)
        0xAFA00014u,                             // sw    zero, 0x14(sp)
        0xAFA00018u,                             // sw    zero, 0x18(sp)
        jal(kRegisterRpcStub),                   // jal   sceSifRegisterRpc
        0x00000000u,                             // nop
        jal(kSleepThreadStub),                   // sleep: jal SleepThread
        0x00000000u,                             // nop
        0x1000FFFDu,                             // b     sleep
        0x00000000u,                             // nop
    };
    const uint32_t thbase[] = {
        0x41E00000u, 0x00000000u, 0x00000101u, 0x61626874u /* "thba" */, 0x00006573u /* "se" */,
        0x03E00008u, 0x24000004u, // CreateThread
        0x03E00008u, 0x24000006u, // StartThread
        0x03E00008u, 0x24000018u, // SleepThread
        0x00000000u, 0x00000000u,
    };
    const uint32_t sifcmd[] = {
        0x41E00000u, 0x00000000u, 0x00000101u, 0x63666973u /* "sifc" */, 0x0000646Du /* "md" */,
        0x03E00008u, 0x24000011u, // sceSifRegisterRpc
        0x00000000u, 0x00000000u,
    };
    const uint32_t descriptor[] = {0x02000000u /* TH_C */, 0u, kLoad + 0x060u, 0x800u, 0x20u};
    const uint32_t serverFunction[] = {0x03E00008u, 0x00000000u}; // jr ra; nop (never called here)
    std::memcpy(elf.data() + codeOffset + 0x000u, start, sizeof(start));
    std::memcpy(elf.data() + codeOffset + 0x060u, thread, sizeof(thread));
    std::memcpy(elf.data() + codeOffset + 0x100u, thbase, sizeof(thbase));
    std::memcpy(elf.data() + codeOffset + 0x140u, sifcmd, sizeof(sifcmd));
    std::memcpy(elf.data() + codeOffset + 0x180u, descriptor, sizeof(descriptor));
    std::memcpy(elf.data() + codeOffset + 0x300u, serverFunction, sizeof(serverFunction));
    return elf;
}

constexpr uint32_t kModuleBuffer = 0x00100000u; // EE RAM: the module image, as SifLoadModuleBuffer passes it
constexpr uint32_t kClient = 0x00200000u;       // EE RAM: t_SifRpcClientData (40 bytes; server at +0x24)
constexpr uint32_t kClientServer = kClient + 0x24u;

struct Bind
{
    bool server;
    double ms;
    bool registeredLater; // the IOP's server table has the SID within 1 s after the bind
};

// One runtime: the IOP thread on, optionally a module load, then one bind (mode 0, waiting) for `sid`.
Bind loadThenBind(uint32_t moduleSid, bool load, uint32_t bindSid)
{
    PS2Runtime runtime;
    require(runtime.memory().initialize(), "memory init");
    uint8_t *rdram = runtime.memory().getRDRAM();
    R5900Context ctx{};
    ps2_syscalls::SifInitRpc(rdram, &ctx, &runtime);
    (runtime.*startIopThreadPtr())();
    require(runtime.iopThreaded(), "the IOP thread did not start");
    if (load)
    {
        const std::vector<uint8_t> irx = threadedRpcServerIrx(moduleSid);
        std::memcpy(rdram + kModuleBuffer, irx.data(), irx.size());
        const ps2x::iop::ModuleLoadResult loaded = runtime.loadIopModuleBuffer(kModuleBuffer);
        require(loaded.handled && loaded.startResult == 0, "the made-up module did not load and start");
    }
    std::memset(rdram + kClient, 0, 40);
    setReg(ctx, 4, kClient);
    setReg(ctx, 5, bindSid);
    setReg(ctx, 6, 0u); // mode 0: wait for the answer
    const auto t0 = std::chrono::steady_clock::now();
    ps2_syscalls::SifBindRpc(rdram, &ctx, &runtime);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    uint32_t server = 0u;
    std::memcpy(&server, rdram + kClientServer, 4);
    bool later = false;
    for (const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
         !later && std::chrono::steady_clock::now() < end;)
    {
        later = (runtime.*canBindIopRpcPtr())(bindSid);
        if (!later)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    (runtime.*stopIopThreadPtr())();
    return {server != 0u, ms, later};
}
} // namespace

int main()
{
    try
    {
        constexpr int kTrials = 20;
        int bound = 0, registered = 0;
        double worstMs = 0.0;
        for (int i = 0; i < kTrials; ++i)
        {
            const uint32_t sid = 0x7E570100u + static_cast<uint32_t>(i);
            const Bind b = loadThenBind(sid, true, sid);
            bound += b.server ? 1 : 0;
            registered += b.registeredLater ? 1 : 0;
            worstMs = std::max(worstMs, b.ms);
        }
        std::cout << "1. bind right after the load, IOP thread on: " << bound << " of " << kTrials
                  << " found the server (slowest bind " << worstMs << " ms); the server was registered by the IOP in "
                  << registered << " of " << kTrials << "\n";
        require(registered == kTrials, "1. the made-up module must register its server once the IOP runs it");
        const Bind none = loadThenBind(0u, false, 0x7E5701FFu);
        std::cout << "2. bind for a server nobody registers: " << (none.server ? "got one (wrong)" : "none")
                  << " after " << none.ms << " ms\n";
        require(bound == kTrials, "1. a bind right after the load must find the server every time");
        require(!none.server, "2. a bind for a missing server must not get one");
        require(none.ms < 1000.0, "2. a bind for a missing server must come back within its budget, not hang");
        std::cout << "iop_bind_after_load: PASS\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "iop_bind_after_load: FAIL " << e.what() << '\n';
        return 1;
    }
}
