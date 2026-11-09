// Authored regression (DBZIW 038): the keyboard's W/A/S/D are the left stick, the arrows the D-pad. Runs the host
// input fixture (PS2X_HOST_INPUT, set by CTest), which goes through the same mapping as real keys, and reads what the
// game would see. No game code or data.
#include "ps2_runtime.h"
#include "runtime/host_input.h"
#include "runtime/ps2_pad.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {
constexpr uint16_t kUp = 0x0010u, kRight = 0x0020u, kDown = 0x0040u, kLeft = 0x0080u;
constexpr uint16_t kDpad = kUp | kRight | kDown | kLeft;
constexpr uint8_t kCentre = 128u, kMin = 1u, kMax = 255u;

int g_failures = 0;

void check(uint64_t tick, const char *what, uint16_t dpad, uint8_t lx, uint8_t ly)
{
    ps2_host_input::poll(tick);
    PSPadBackend backend;
    uint8_t data[32]{};
    if (!backend.readState(0, 0, data, sizeof(data), tick)) // 013 (DW5 023 5c): the sample of field `tick`
    {
        std::cerr << "FAILED: " << what << ": readState\n";
        ++g_failures;
        return;
    }
    const uint16_t pressed = static_cast<uint16_t>(~(data[2] | (data[3] << 8)));
    const uint16_t gotDpad = pressed & kDpad;
    if (gotDpad != dpad || data[6] != lx || data[7] != ly || data[4] != kCentre || data[5] != kCentre)
    {
        std::cerr << "FAILED: " << what << ": dpad " << gotDpad << " (want " << dpad << "), lx " << int(data[6]) << " (want "
                  << int(lx) << "), ly " << int(data[7]) << " (want " << int(ly) << "), rx " << int(data[4]) << ", ry "
                  << int(data[5]) << '\n';
        ++g_failures;
    }
}
}

int main()
{
    if (!std::getenv("PS2X_HOST_INPUT"))
    {
        std::cerr << "FAILED: PS2X_HOST_INPUT is not set (CTest sets it)\n";
        return 1;
    }
    check(1, "nothing pressed", 0u, kCentre, kCentre);
    check(100, "W", 0u, kCentre, kMin);
    check(200, "S", 0u, kCentre, kMax);
    check(300, "A", 0u, kMin, kCentre);
    check(400, "D", 0u, kMax, kCentre);
    check(500, "W+A", 0u, kMin, kMin);
    check(600, "W+S cancel", 0u, kCentre, kCentre);
    check(700, "A+D cancel", 0u, kCentre, kCentre);
    check(800, "UP", kUp, kCentre, kCentre);
    check(900, "DOWN", kDown, kCentre, kCentre);
    check(1000, "LEFT", kLeft, kCentre, kCentre);
    check(1100, "RIGHT", kRight, kCentre, kCentre);
    check(1200, "W with the arrow DOWN", kDown, kCentre, kMin);
    check(1300, "released again", 0u, kCentre, kCentre);
    if (g_failures)
        return 1;
    std::cout << "keyboard_stick: all cases OK\n";
    return 0;
}
