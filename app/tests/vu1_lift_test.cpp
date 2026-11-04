// vu1_lift (issue 012; DW5 025 and DW5 036, 19360e1): every translated VU1 program against the interpreter.
// For each target in app/setup/vu1lift/vu1_inputs.h the 16 KB VU1 image is rebuilt from the player's own executable
// (DBZIW_TEST_ELF, set by CTest; offsets and checksums only, no game data stored with the test). The program runs
// once interpreted and once translated, through the real dispatch, from the same start state and VU memory: the
// translated code must have run (lastRunLifted, so a lookup miss cannot compare the interpreter with itself), and
// registers, flags, Q, CLIP, pc, cycle count and the 16 KB data memory must be identical.
//
//   dbziw_vu1_lift_test                 all targets (CTest vu1_lift)
//   dbziw_vu1_lift_test replaycap <stem> ...
//                                       one saved run each (PS2X_VU1_VERIFY_DUMP / PS2X_VU1_RUN_CAPTURE files):
//                                       native against interpreted; game data, local only
// DW5's dma-cancel modes are 018's (vu1_dma_reset_test.cpp); DW5's benchmarks stay in DW5.
#include "runtime/ps2_vu1.h"
#include "runtime/ps2_vu1_lift.h"
#include "ps2_runtime.h"
#include "setup/vu1lift/vu1_inputs.h"
#include "setup/vu1lift/vu1_translator.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {

void require(bool condition, const std::string &msg)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << msg << std::endl;
        throw std::runtime_error(msg);
    }
}

std::vector<uint8_t> loadFile(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Could not open file: " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

const std::map<std::string, std::vector<uint8_t>> &discImages()
{
    static const std::map<std::string, std::vector<uint8_t>> images = []
    {
        const char *elf = std::getenv("DBZIW_TEST_ELF");
        require(elf && *elf, "DBZIW_TEST_ELF must name the player's SLES_553.47 (set by CTest)");
        std::map<std::string, std::vector<uint8_t>> loaded;
        std::string error;
        require(dbziw::vu1lift::loadVu1Images(elf, loaded, error), std::string("VU1 images from ") + elf + ": " + error);
        return loaded;
    }();
    return images;
}

struct Result
{
    VU1State state;
    std::vector<uint8_t> data;
    bool lifted;
};

// One run of `code` from `start`/`data` on a fresh runtime, translated (1) or interpreted (0).
Result runOnce(const std::vector<uint8_t> &code, const std::vector<uint8_t> &data, const VU1State &start, uint32_t pc,
               uint32_t top, uint32_t itop, int lifted, uint32_t budget)
{
    PS2Runtime rt;
    require(rt.memory().initialize(), "memory initialise");
    std::memcpy(rt.memory().getVU1Code(), code.data(), 16384);
    std::memcpy(rt.memory().getVU1Data(), data.data(), 16384);
    rt.vu1().state() = start;
    vl::setEnabledOverride(lifted);
    rt.vu1().execute(rt.memory().getVU1Code(), 16384, rt.memory().getVU1Data(), 16384, rt.gs(), &rt.memory(), pc, top,
                     itop, budget);
    vl::setEnabledOverride(-1);
    const uint8_t *out = rt.memory().getVU1Data();
    return {rt.vu1().state(), std::vector<uint8_t>(out, out + 16384), rt.vu1().lastRunLifted()};
}

// The first difference between two end states, or "" when they are identical.
std::string difference(const Result &a, const Result &b)
{
    const VU1State &x = a.state, &y = b.state;
    if (x.pc != y.pc)
        return "pc " + std::to_string(x.pc) + " vs " + std::to_string(y.pc);
    if (x.cycles != y.cycles)
        return "cycles " + std::to_string(x.cycles) + " vs " + std::to_string(y.cycles);
    if (std::memcmp(x.vf, y.vf, sizeof(x.vf)) != 0)
        return "VF registers";
    for (int i = 0; i < 16; ++i)
        if ((x.vi[i] & 0xFFFFu) != (y.vi[i] & 0xFFFFu))
            return "VI[" + std::to_string(i) + "]";
    if (std::memcmp(x.acc, y.acc, sizeof(x.acc)) != 0)
        return "ACC";
    if (std::memcmp(&x.q, &y.q, sizeof(x.q)) != 0)
        return "Q";
    if (x.mac != y.mac || x.status != y.status || (x.clip & 0xFFFFFFu) != (y.clip & 0xFFFFFFu))
        return "MAC/status/CLIP flags";
    if (a.data != b.data)
        return "data memory";
    return "";
}

// A reproducible start: VU memory with small loop counts and ordinary floats, registers with distinct values.
void syntheticStart(std::vector<uint8_t> &data, VU1State &state)
{
    data.assign(16384, 0);
    for (size_t q = 0; q < 1024; ++q)
    {
        float *f = reinterpret_cast<float *>(data.data() + q * 16);
        for (int c = 0; c < 4; ++c)
            f[c] = 0.5f + 0.25f * static_cast<float>((q * 7 + static_cast<size_t>(c) * 3) % 16);
        uint32_t w;
        std::memcpy(&w, &f[0], 4);
        w = (w & ~0x7FFFu) | 1u; // low halfword 1: loop and vertex counts read as 1
        std::memcpy(&f[0], &w, 4);
    }
    state = VU1State{};
    for (int i = 1; i < 32; ++i)
        for (int c = 0; c < 4; ++c)
            state.vf[i][c] = 1.0f + 0.1f * static_cast<float>(i) + static_cast<float>(c);
    state.vf[0][3] = 1.0f;
    state.q = 1.0f;
    state.i = 255.0f;
    for (int i = 1; i < 16; ++i)
        state.vi[i] = static_cast<uint16_t>(i * 16);
}

void testGenerations()
{
    // The translation lookup is cached per thread by code pointer and generation: two memories (possibly at the
    // same address) must never share a generation (DW5 023, dfff5a6).
    PS2Runtime first, second;
    require(first.memory().initialize() && second.memory().initialize(), "memories initialise");
    require(first.memory().getVU1CodeGeneration() != second.memory().getVU1CodeGeneration(),
            "two memories must not share a VU code generation");
}

int testAllTargets()
{
    constexpr uint32_t kBudget = 400000000u; // VU1 cycles: beyond any program run on its synthetic input
    int failed = 0;
    for (const auto &t : dbziw::vu1lift::kTargets)
    {
        const auto it = discImages().find(t.key);
        require(it != discImages().end(), std::string("no rebuilt image for ") + t.name);
        std::vector<uint8_t> data;
        VU1State start;
        syntheticStart(data, start);
        const Result interp = runOnce(it->second, data, start, t.pc, 0, 0, 0, kBudget);
        const Result native = runOnce(it->second, data, start, t.pc, 0, 0, 1, kBudget);
        std::string problem;
        if (!native.lifted)
            problem = "translated code did not run (lookup on the rebuilt image)";
        else if (interp.state.cycles - start.cycles >= kBudget)
            problem = "the interpreter did not finish within the budget";
        else
            problem = difference(native, interp);
        std::cout << (problem.empty() ? "  PASS " : "  FAIL ") << t.name << ": " << (interp.state.cycles - start.cycles)
                  << " cycles" << (problem.empty() ? "" : ", " + problem + " (translated: " +
                                   std::to_string(native.state.cycles - start.cycles) + " cycles, pc " +
                                   std::to_string(native.state.pc) + "; interpreter pc " +
                                   std::to_string(interp.state.pc) + ", same data " +
                                   (native.data == interp.data ? "yes" : "no") + ")") << "\n";
        failed += problem.empty() ? 0 : 1;
    }
    // The translator's timing mirror must match the interpreter's decode, or segments silently run interpreted.
    std::cout << "declined segments: " << vl::declinedSegments() << "\n";
    if (vl::declinedSegments() != 0)
        ++failed;
    return failed;
}

// Native against interpreted for one saved run; false on a difference.
bool replayCaptured(const std::string &stem)
{
    const auto code = loadFile(stem + ".code"), image = loadFile(stem + ".data"), stateBytes = loadFile(stem + ".state");
    require(code.size() == 16384 && image.size() == 16384 && stateBytes.size() == sizeof(VU1State), "capture files");
    VU1State initial{};
    std::memcpy(&initial, stateBytes.data(), sizeof(initial));
    const Result native = runOnce(code, image, initial, initial.pc, initial.top, initial.itop, 1, 100000);
    const Result interp = runOnce(code, image, initial, initial.pc, initial.top, initial.itop, 0, 100000);
    const std::string problem = !native.lifted ? "no translated code for this run" : difference(native, interp);
    std::cout << stem << ": native cycles " << (native.state.cycles - initial.cycles) << ", interpreter "
              << (interp.state.cycles - initial.cycles) << (problem.empty() ? " (identical)" : " (" + problem + ")")
              << "\n";
    return problem.empty();
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc >= 3 && std::string(argv[1]) == "replaycap")
        {
            bool all = true;
            for (int i = 2; i < argc; ++i)
                all = replayCaptured(argv[i]) && all;
            return all ? 0 : 1;
        }
        require(vl::find(0xdeadbeefull, 0x0000) == nullptr, "an unregistered hash finds nothing");
        testGenerations();
        const int failed = testAllTargets();
        std::cout << dbziw::vu1lift::kTargets.size() << " targets, " << failed << " failed\n";
        return failed == 0 ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
