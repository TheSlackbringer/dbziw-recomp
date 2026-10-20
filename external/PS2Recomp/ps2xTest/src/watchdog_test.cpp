#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

namespace
{
    void stallingGuestFunction(uint8_t * /*rdram*/, R5900Context * /*ctx*/, PS2Runtime * /*runtime*/)
    {
        // Loop indefinitely in guest code without yielding to trigger the watchdog
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

int main(int /*argc*/, char ** /*argv*/)
{
    std::vector<uint8_t> rdram(PS2_RAM_SIZE, 0);
    PS2Runtime runtime;

    constexpr uint32_t kLoopPc = 0x00100500u;
    constexpr uint32_t kCallerPc = 0x00100250u;
    constexpr uint32_t kStackPtr = 0x01FFE000u;

    runtime.registerFunction(kLoopPc, stallingGuestFunction);

    // Place a return address / caller frame onto the guest stack
    *reinterpret_cast<uint32_t *>(&rdram[kStackPtr & PS2_RAM_MASK]) = kCallerPc;

    R5900Context mainContext{};
    std::memset(&mainContext, 0, sizeof(mainContext));
    mainContext.pc = kLoopPc;
    mainContext.r[29] = _mm_set_epi64x(0, static_cast<int64_t>(kStackPtr));
    mainContext.r[31] = _mm_set_epi64x(0, static_cast<int64_t>(kCallerPc));

    runtime.eeScheduler().reset(rdram.data(), mainContext);
    runtime.eeScheduler().run();

    return 0;
}
