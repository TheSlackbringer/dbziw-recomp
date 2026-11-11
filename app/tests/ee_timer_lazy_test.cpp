// DBZIW 039 (v3 stage 3): the EE timers (T0-T3) are advanced only when something can see them: at the cycle of their
// next possible interrupt, or when the game reads or writes a timer register. This test drives PS2Memory with random
// timer setups, register writes and reads, and cycle steps of every size, beside a copy of the model as it was before
// (every accounted step applied at once), and requires the same interrupts at the same step, the same register reads
// and the same cycles-to-next-interrupt after every operation. No game code or data.
#include "ps2_runtime.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace
{
    // The per-step model before 039 (ps2_memory.cpp at f832a39), unchanged.
    constexpr std::array<uint32_t, 4> kBases = {0x10000000u, 0x10000800u, 0x10001000u, 0x10001800u};
    constexpr uint32_t kCount = 0x00u, kMode = 0x10u, kCompare = 0x20u, kHold = 0x30u;
    constexpr uint32_t kClksMask = 0x3u, kConfigMask = 0x3FFu, kStatusMask = 0xC00u;
    constexpr uint32_t kZret = 1u << 6, kCue = 1u << 7, kCmpe = 1u << 8, kOvfe = 1u << 9, kEquf = 1u << 10,
                       kOvff = 1u << 11;
    constexpr uint64_t kEeClockHz = 294912000ull;
    constexpr std::array<uint64_t, 4> kClockHz = {147456000ull, 9216000ull, 576000ull, 15734ull};

    constexpr uint64_t ticksUntilMatch(uint32_t count, uint32_t target)
    {
        const uint32_t distance = (target - count) & 0xFFFFu;
        return distance == 0u ? 0x10000ull : static_cast<uint64_t>(distance);
    }

    struct Timer
    {
        uint32_t count = 0, mode = 0, compare = 0, hold = 0;
        uint64_t clockRemainder = 0;
    };

    struct Reference
    {
        std::array<Timer, 4> timers{};

        uint32_t advance(uint64_t eeCycles)
        {
            if (eeCycles == 0u)
                return 0u;
            uint32_t interruptMask = 0u;
            for (size_t index = 0; index < timers.size(); ++index)
            {
                Timer &timer = timers[index];
                if ((timer.mode & kCue) == 0u)
                    continue;
                const uint64_t clockHz = kClockHz[timer.mode & kClksMask];
                const uint64_t wholeSeconds = eeCycles / kEeClockHz;
                const uint64_t remainingCycles = eeCycles % kEeClockHz;
                const uint64_t scaled = remainingCycles * clockHz + timer.clockRemainder;
                const uint64_t ticks = wholeSeconds * clockHz + scaled / kEeClockHz;
                timer.clockRemainder = scaled % kEeClockHz;
                if (ticks == 0u)
                    continue;
                const uint32_t oldCount = timer.count & 0xFFFFu;
                const uint32_t compare = timer.compare & 0xFFFFu;
                const uint64_t compareDistance = ticksUntilMatch(oldCount, compare);
                const uint64_t overflowDistance = 0x10000ull - oldCount;
                const bool zeroReturn = (timer.mode & kZret) != 0u;
                const bool compareReached = ticks >= compareDistance;
                bool overflowReached = false;
                if (zeroReturn)
                {
                    overflowReached = ticks >= overflowDistance && overflowDistance <= compareDistance;
                    if (compareReached)
                    {
                        const uint64_t remaining = ticks - compareDistance;
                        timer.count = compare == 0u ? static_cast<uint32_t>(remaining & 0xFFFFu)
                                                    : static_cast<uint32_t>(remaining % compare);
                    }
                    else
                        timer.count = static_cast<uint32_t>((oldCount + ticks) & 0xFFFFu);
                }
                else
                {
                    overflowReached = ticks >= overflowDistance;
                    timer.count = static_cast<uint32_t>((oldCount + ticks) & 0xFFFFu);
                }
                if (compareReached && (timer.mode & kCmpe) != 0u && (timer.mode & kEquf) == 0u)
                {
                    timer.mode |= kEquf;
                    interruptMask |= 1u << index;
                }
                if (overflowReached && (timer.mode & kOvfe) != 0u && (timer.mode & kOvff) == 0u)
                {
                    timer.mode |= kOvff;
                    interruptMask |= 1u << index;
                }
            }
            return interruptMask;
        }

        uint64_t cyclesUntilNext() const
        {
            uint64_t nearest = std::numeric_limits<uint64_t>::max();
            for (const Timer &timer : timers)
            {
                if ((timer.mode & kCue) == 0u)
                    continue;
                const uint32_t count = timer.count & 0xFFFFu;
                const uint32_t compare = timer.compare & 0xFFFFu;
                const uint64_t compareDistance = ticksUntilMatch(count, compare);
                const uint64_t overflowDistance = 0x10000ull - count;
                uint64_t eventTicks = std::numeric_limits<uint64_t>::max();
                if ((timer.mode & kCmpe) != 0u && (timer.mode & kEquf) == 0u)
                    eventTicks = compareDistance;
                const bool overflowCanOccur = (timer.mode & kZret) == 0u || overflowDistance <= compareDistance;
                if (overflowCanOccur && (timer.mode & kOvfe) != 0u && (timer.mode & kOvff) == 0u)
                    eventTicks = std::min(eventTicks, overflowDistance);
                if (eventTicks == std::numeric_limits<uint64_t>::max())
                    continue;
                const uint64_t clockHz = kClockHz[timer.mode & kClksMask];
                const uint64_t numerator = eventTicks * kEeClockHz - timer.clockRemainder;
                const uint64_t cycles = (numerator + clockHz - 1u) / clockHz;
                nearest = std::min(nearest, std::max<uint64_t>(1u, cycles));
            }
            return nearest;
        }

        void write(size_t index, uint32_t offset, uint32_t value)
        {
            Timer &timer = timers[index];
            switch (offset)
            {
            case kCount:
                timer.count = value & 0xFFFFu;
                timer.clockRemainder = 0u;
                break;
            case kMode:
            {
                const uint32_t previousMode = timer.mode;
                const uint32_t status = (previousMode & kStatusMask) & ~(value & kStatusMask);
                timer.mode = (value & kConfigMask) | status;
                if (((previousMode ^ timer.mode) & (kClksMask | kCue)) != 0u)
                    timer.clockRemainder = 0u;
                break;
            }
            case kCompare:
                timer.compare = value & 0xFFFFu;
                break;
            case kHold:
                timer.hold = value & 0xFFFFu;
                break;
            }
        }

        uint32_t read(size_t index, uint32_t offset) const
        {
            const Timer &timer = timers[index];
            switch (offset)
            {
            case kCount: return timer.count & 0xFFFFu;
            case kMode: return timer.mode & (kConfigMask | kStatusMask);
            case kCompare: return timer.compare & 0xFFFFu;
            case kHold: return timer.hold & 0xFFFFu;
            }
            return 0u;
        }
    };

    void require(bool condition, const std::string &message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    uint64_t stepSize(std::mt19937_64 &rng)
    {
        switch (rng() % 16u)
        {
        case 0: return 1u + rng() % 3u;
        case 1: case 2: case 3: case 4: case 5: return 1u + rng() % 64u;      // dispatch-sized steps
        case 6: case 7: case 8: return 1u + rng() % 4096u;
        case 9: case 10: return 1u + rng() % 200000u;
        case 11: return 1u + rng() % 5000000u;
        case 12: return kEeClockHz - 2u + rng() % 5u;                     // around one second
        case 13: return 1u + rng() % (3u * kEeClockHz);                    // up to three seconds
        default: return 1u + rng() % 600u;
        }
    }

    uint32_t randomMode(std::mt19937_64 &rng)
    {
        uint32_t mode = static_cast<uint32_t>(rng()) & kConfigMask;
        if (rng() % 4u != 0u)
            mode |= kCue;
        if (rng() % 3u == 0u)
            mode |= kStatusMask & static_cast<uint32_t>(rng()); // write 1 to clear EQUF / OVFF
        return mode;
    }

    uint32_t randomCompare(std::mt19937_64 &rng)
    {
        switch (rng() % 6u)
        {
        case 0: return 0u;
        case 1: return 1u + rng() % 4u;
        case 2: return 0xFFFFu - rng() % 4u;
        default: return static_cast<uint32_t>(rng() & 0xFFFFu);
        }
    }
}

int main()
{
    try
    {
        uint64_t operations = 0u, interrupts = 0u, steps = 0u;
        for (uint64_t seed = 1u; seed <= 400u; ++seed)
        {
            PS2Memory memory;
            require(memory.initialize(), "memory initialization");
            memory.resetEeTimers();
            Reference reference;
            std::mt19937_64 rng(seed);
            const auto check = [&](const std::string &where)
            {
                require(memory.cyclesUntilNextEeTimerInterrupt() == reference.cyclesUntilNext(),
                        "seed " + std::to_string(seed) + " " + where + ": cycles to next interrupt " +
                            std::to_string(memory.cyclesUntilNextEeTimerInterrupt()) + " vs " +
                            std::to_string(reference.cyclesUntilNext()));
            };
            for (int op = 0; op < 4000; ++op, ++operations)
            {
                const uint32_t kind = static_cast<uint32_t>(rng() % 100u);
                const size_t index = static_cast<size_t>(rng() % 4u);
                const std::string where = "op " + std::to_string(op);
                if (kind < 70u)
                {
                    const uint64_t cycles = stepSize(rng);
                    const uint32_t got = memory.advanceEeTimers(cycles);
                    const uint32_t want = reference.advance(cycles);
                    require(got == want, "seed " + std::to_string(seed) + " " + where + ": step of " +
                                             std::to_string(cycles) + " raised " + std::to_string(got) + " vs " +
                                             std::to_string(want));
                    interrupts += want != 0u;
                    ++steps;
                }
                else if (kind < 85u)
                {
                    const uint32_t offsets[] = {kCount, kMode, kCompare, kHold};
                    const uint32_t offset = offsets[rng() % (index < 2u ? 4u : 3u)];
                    const uint32_t got = memory.readIORegister(kBases[index] + offset);
                    const uint32_t want = reference.read(index, offset);
                    require(got == want, "seed " + std::to_string(seed) + " " + where + ": read T" +
                                             std::to_string(index) + "+0x" + std::to_string(offset) + " " +
                                             std::to_string(got) + " vs " + std::to_string(want));
                }
                else
                {
                    const uint32_t which = static_cast<uint32_t>(rng() % 10u);
                    const uint32_t offset = which < 5u ? kMode : (which < 8u ? kCompare : (which < 9u ? kCount : kHold));
                    if (offset == kHold && index >= 2u)
                        continue;
                    const uint32_t value = offset == kMode      ? randomMode(rng)
                                           : offset == kCompare ? randomCompare(rng)
                                                                : static_cast<uint32_t>(rng() & 0xFFFFu);
                    memory.writeIORegister(kBases[index] + offset, value);
                    reference.write(index, offset, value);
                }
                check(where);
            }
        }
        require(interrupts > 1000u, "too few interrupts exercised: " + std::to_string(interrupts));
        std::cout << "ee_timer_lazy: PASS (" << operations << " operations, " << steps << " steps, " << interrupts
                  << " steps raising interrupts)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "ee_timer_lazy: FAIL: " << error.what() << '\n';
        return 1;
    }
}
