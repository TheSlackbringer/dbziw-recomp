// DBZIW-TEST: the EE PollSema contract and the game's memory-card library on top of it (issue 018; the contract
// part from DW5's ee_poll_sema test at 438a3de).
//
// 1. The scheduler and both syscalls (PollSema 0x45, iPollSema -0x46): an empty or unknown semaphore gives exactly
//    -1, sign-extended to 64 bits; a successful poll returns the ID and takes one token.
// 2. The game's libmc write call (FUN_00113948: descriptor, buffer, size; the buffer's cache is written back before
//    the RPC, so sceMcWrite) runs as generated from the player's disc, through the runtime's PollSema/SignalSema
//    bindings (0x101df0, 0x101dc0). It tests PollSema's result with a 64-bit `bgez`, so a failure that is not
//    sign-extended would start an RPC on a busy card. Busy card: -200 and no RPC. Free card: one RPC (command 0x1B)
//    and the token kept for the RPC's end callback. Failed RPC: the token given back. sceSifWriteBackDCache and
//    sceSifCallRpc are fakes that record their arguments. No game data is embedded: the generated functions are
//    local build inputs.
#include "ps2_runtime.h"
#include "ps2_syscalls.h"
#include "runtime/ee_scheduler.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

void FUN_00113948_0x113948(uint8_t *, R5900Context *, PS2Runtime *);
void PollSema_0x101df0(uint8_t *, R5900Context *, PS2Runtime *);
void SignalSema_0x101dc0(uint8_t *, R5900Context *, PS2Runtime *);

namespace {
constexpr uint32_t kSignalSema = 0x101dc0u;
constexpr uint32_t kPollSema = 0x101df0u;
constexpr uint32_t kWriteBackDCache = 0x105438u;
constexpr uint32_t kCallRpc = 0x105e70u;
constexpr uint32_t kTableEnd = kCallRpc + 4u;
// libmc's state, as FUN_00113948 addresses it.
constexpr uint32_t kClientBound = 0x4a52a4u; // the RPC client (0x4a5280) + 0x24: non-zero once bound
constexpr uint32_t kSemaId = 0x40f04cu;      // the card semaphore's ID
constexpr uint32_t kCommand = 0x40f048u;     // the command in flight
constexpr uint32_t kParams = 0x4a5300u;      // the RPC's send buffer, 0x30 bytes
constexpr uint32_t kCaller = 0x1000u;
constexpr uint32_t kUntouched = 0xa5a5a5a5u;

struct Calls {
    int writeBacks = 0, rpcs = 0;
    uint32_t writeBack[2]{}, rpc[8]{};
    uint64_t rpcResult = 0;
} g_calls;

void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
uint32_t reg(const R5900Context &ctx, unsigned index) {
    uint32_t value; std::memcpy(&value, &ctx.r[index], sizeof(value)); return value;
}
uint64_t reg64(const R5900Context &ctx, unsigned index) {
    uint64_t value; std::memcpy(&value, &ctx.r[index], sizeof(value)); return value;
}
void setReg(R5900Context &ctx, unsigned index, uint64_t value) {
    ctx.r[index] = _mm_setzero_si128();
    std::memcpy(&ctx.r[index], &value, sizeof(value));
}
uint32_t word(const std::vector<uint8_t> &ram, uint32_t address) {
    uint32_t value; std::memcpy(&value, ram.data() + address, sizeof(value)); return value;
}
void setWord(std::vector<uint8_t> &ram, uint32_t address, uint32_t value) {
    std::memcpy(ram.data() + address, &value, sizeof(value));
}
uint64_t sext(int64_t value) { return static_cast<uint64_t>(value); }

void fakeWriteBackDCache(uint8_t *, R5900Context *ctx, PS2Runtime *) {
    ++g_calls.writeBacks;
    g_calls.writeBack[0] = reg(*ctx, 4);
    g_calls.writeBack[1] = reg(*ctx, 5);
    ctx->pc = reg(*ctx, 31);
}
void fakeCallRpc(uint8_t *, R5900Context *ctx, PS2Runtime *) {
    ++g_calls.rpcs;
    for (unsigned i = 0; i < 8; ++i) g_calls.rpc[i] = reg(*ctx, 4 + i); // a0-a3, t0-t3
    setReg(*ctx, 2, g_calls.rpcResult);
    ctx->pc = reg(*ctx, 31);
}
}

// The dense function table from the semaphore bindings to sceSifCallRpc; main() fills the four entries.
extern const uint32_t g_ps2RecompiledFunctionTableBase = kSignalSema;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = kTableEnd;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = (kTableEnd - kSignalSema) / 4u;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(kTableEnd - kSignalSema) / 4u] = {};

int main() {
    try {
        PS2Runtime runtime;
        R5900Context context{};
        std::vector<uint8_t> ram(0x2000000u);
        auto &kernel = runtime.eeScheduler();
        kernel.bindMainContextForSyscall(context, ram.data());
        const int sema = kernel.createSemaphore(0, 1, 0, 0);
        require(sema > 0, "create empty semaphore");

        // 1. The scheduler and both syscalls.
        require(kernel.pollSemaphore(sema) == -1, "empty poll must return exactly -1");
        require(kernel.pollSemaphore(-123) == -1, "unknown ID must return exactly -1");
        for (uint32_t syscall : {0x45u, static_cast<uint32_t>(-0x46)}) {
            setReg(context, 4, static_cast<uint32_t>(sema));
            require(ps2_syscalls::dispatchNumericSyscall(syscall, ram.data(), &context, &runtime),
                    "dispatch PollSema/iPollSema");
            require(reg64(context, 2) == UINT64_MAX, "empty poll: exactly -1, sign-extended to 64 bits");
            require(kernel.signalSemaphore(sema, false) >= 0, "signal before a successful poll");
            setReg(context, 4, static_cast<uint32_t>(sema));
            require(ps2_syscalls::dispatchNumericSyscall(syscall, ram.data(), &context, &runtime),
                    "dispatch successful poll");
            require(reg(context, 2) == static_cast<uint32_t>(sema), "successful poll returns the ID");
            require(kernel.pollSemaphore(sema) == -1, "a successful poll takes exactly one token");
            setReg(context, 4, 0xffffff85u);
            require(ps2_syscalls::dispatchNumericSyscall(syscall, ram.data(), &context, &runtime),
                    "dispatch invalid ID poll");
            require(reg64(context, 2) == UINT64_MAX, "invalid ID: exactly -1, sign-extended to 64 bits");
        }

        // 2. The game's memory-card write, through the runtime's semaphore bindings.
        require(runtime.registerFunction(kPollSema, PollSema_0x101df0) &&
                    runtime.registerFunction(kSignalSema, SignalSema_0x101dc0) &&
                    runtime.registerFunction(kWriteBackDCache, fakeWriteBackDCache) &&
                    runtime.registerFunction(kCallRpc, fakeCallRpc),
                "register the bindings and fakes");
        setWord(ram, kSemaId, static_cast<uint32_t>(sema));
        const uint32_t fd = 3u, buffer = 0x200000u, size = 0x2000u;
        auto write = [&] {
            setWord(ram, kCommand, kUntouched);
            for (uint32_t offset = 0; offset < 0x30u; offset += 4u) setWord(ram, kParams + offset, kUntouched);
            context.pc = 0x113948u;
            setReg(context, 4, fd);
            setReg(context, 5, buffer);
            setReg(context, 6, size);
            setReg(context, 29, 0x1fff000u);
            setReg(context, 31, kCaller);
            FUN_00113948_0x113948(ram.data(), &context, &runtime);
            require(context.pc == kCaller, "the library call returns to its caller");
            return reg64(context, 2);
        };

        setWord(ram, kClientBound, 0u);
        require(write() == sext(-100), "client not bound: -100");
        require(g_calls.rpcs == 0 && g_calls.writeBacks == 0, "client not bound: no RPC");

        setWord(ram, kClientBound, 1u);
        require(write() == sext(-200), "busy card (empty semaphore): -200");
        require(g_calls.rpcs == 0 && g_calls.writeBacks == 0, "busy card: no RPC and no cache write-back");
        require(word(ram, kCommand) == kUntouched && word(ram, kParams) == kUntouched,
                "busy card: the command and the RPC parameters are left alone");
        require(kernel.pollSemaphore(sema) == -1, "busy card: the semaphore stays empty");

        require(kernel.signalSemaphore(sema, false) >= 0, "the card becomes free");
        g_calls.rpcResult = 0u;
        require(write() == 0u, "free card: the write is started");
        require(g_calls.writeBacks == 1 && g_calls.writeBack[0] == buffer && g_calls.writeBack[1] == size,
                "free card: the buffer's cache is written back once");
        require(g_calls.rpcs == 1, "free card: one RPC");
        const uint32_t expected[8] = {0x4a5280u, 0x1bu, 1u, kParams, 0x30u, 0x4a6840u, 4u, 0x111d88u};
        require(std::memcmp(g_calls.rpc, expected, sizeof(expected)) == 0,
                "free card: RPC to the card client, command 0x1B, no wait, 0x30-byte parameters, end callback");
        require(word(ram, kParams) == fd && word(ram, kParams + 0xcu) == size &&
                    word(ram, kParams + 0x18u) == buffer && word(ram, kParams + 0x1cu) == 0x4a5780u,
                "free card: descriptor, size and buffer in the RPC parameters");
        require(word(ram, kCommand) == 0x1bu, "free card: command 0x1B in flight");
        require(kernel.pollSemaphore(sema) == -1, "free card: the token stays taken until the RPC ends");

        require(kernel.signalSemaphore(sema, false) >= 0, "the card is free again");
        g_calls.rpcResult = UINT64_MAX;
        require(write() == UINT64_MAX, "failed RPC: its result is returned");
        require(g_calls.rpcs == 2, "failed RPC: one more RPC");
        require(word(ram, kCommand) == kUntouched, "failed RPC: no command in flight");
        require(kernel.pollSemaphore(sema) == sema, "failed RPC: the token is given back");
        require(kernel.pollSemaphore(sema) == -1, "failed RPC: exactly one token given back");

        std::cout << "EE PollSema contract and the game's memory-card write passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
