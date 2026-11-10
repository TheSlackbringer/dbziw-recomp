#pragma once
// DBZIW 039 (v3 stage 1): a short spin before a thread sleeps on a condition variable. The game thread, the device
// (VU1) worker and the GS worker hand work to each other ~90 times per 60 Hz field, with gaps of 0.01-0.07 ms (039's
// profile, tests/out/039-profile-1x-20261110-092259): sleeping and being woken by a system call each time cost the
// game thread ~8 % and the device worker ~8 % of their time. A consumer that spins for up to kHandoverSpin first is
// still awake when the next piece of work arrives, so neither side makes the system call. Host scheduling only: what
// the game sees (order, results, guest time) does not change. The same bound on every PC; not a game timing knob.
#include <chrono>
#include <cstdint>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

namespace ps2x
{
    inline constexpr std::chrono::microseconds kHandoverSpin{50};

    inline void cpuRelax()
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_pause();
#else
        std::this_thread::yield();
#endif
    }

    // Spins until ready() or until `limit` has passed; returns ready()'s last answer. ready() must be cheap and
    // lock-free (an atomic load).
    template <class Ready>
    bool spinUntil(Ready ready, std::chrono::nanoseconds limit = kHandoverSpin)
    {
        if (ready())
            return true;
        const auto start = std::chrono::steady_clock::now();
        for (uint32_t i = 1;; ++i)
        {
            cpuRelax();
            if (ready())
                return true;
            if ((i & 63u) == 0u && std::chrono::steady_clock::now() - start >= limit)
                return ready();
        }
    }
} // namespace ps2x
