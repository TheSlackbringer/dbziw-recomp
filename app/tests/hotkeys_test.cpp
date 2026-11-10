// DBZIW 013 (DW5 eaa68f3 / 08d30e8; you, 2026-11-08: "b", DW5's layout): the game window's function keys: F1 debugger,
// F2 frame rate, F3 anti-aliasing, F4 resolution, F8 build ID and game tick (DW5's F5/F9 save/load state and F10 Discs
// window are not bound in DBZIW: issue 016 brings save states, and DBZIW has one disc). Checks the layout, that no key
// has two jobs, that README.md's key table lists exactly these keys, and that no handler names a KEY_Fn itself.
#include "runtime/hotkeys.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <string>

using namespace ps2x::hotkeys;

namespace {
int failures = 0;
void check(bool ok, const std::string &what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    failures += ok ? 0 : 1;
}
} // namespace

int main()
{
    struct Want { Action action; int key; const char *what; };
    const Want layout[] = {
        {Action::Debugger, KEY_F1, "debugger on F1"},          {Action::FpsOverlay, KEY_F2, "frame rate on F2"},
        {Action::AntiAliasing, KEY_F3, "anti-aliasing on F3"}, {Action::OutputScale, KEY_F4, "resolution on F4"},
        {Action::BuildOverlay, KEY_F8, "build ID and game tick on F8"},
    };
    for (const Want &w : layout)
    {
        int bindings = 0;
        for (const Binding &b : kBindings)
            bindings += b.action == w.action ? 1 : 0;
        check(bindings == 1 && key(w.action) == w.key, w.what);
    }
    check(std::size(kBindings) == std::size(layout), "every binding is in the layout");

    std::set<int> keys;
    for (const Binding &b : kBindings)
        keys.insert(b.key);
    check(keys.size() == std::size(kBindings), "no key has two jobs");

    // README.md: every "| **Fn** |" row of the key table is a bound key, and every bound key has a row. A run without
    // the source tree (an installed test program) skips these checks.
    std::ifstream readme(DBZIW_SOURCE_DIR "/README.md");
    if (!readme.good())
    {
        std::printf("SKIP  README.md and handler sources not available here\n");
        std::printf("%s\n", failures ? "HOTKEY TESTS FAILED" : "ALL HOTKEY TESTS PASSED");
        return failures ? 1 : 0;
    }
    std::set<std::string> documented;
    const std::regex row(R"(^\|\s*\*\*(F[0-9]{1,2})\*\*)");
    for (std::string line; std::getline(readme, line);)
        if (std::smatch m; std::regex_search(line, m, row))
            documented.insert(m[1]);
    std::set<std::string> bound;
    for (const Binding &b : kBindings)
        bound.insert(b.name);
    check(documented == bound, "README.md key table lists exactly the bound keys");

    // The handlers take their keys from the table: no hard-coded IsKeyPressed(KEY_Fn) left.
    for (const char *source : {"/external/PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp",
                               "/external/PS2Recomp/ps2xRuntime/src/lib/ps2_debug_panel.cpp"})
    {
        std::ifstream file(std::string(DBZIW_SOURCE_DIR) + source);
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        check(!text.empty(), std::string(source) + " found");
        check(!std::regex_search(text, std::regex(R"(IsKeyPressed\(\s*KEY_F[0-9])")),
              std::string(source) + " reads its function keys from the table");
    }

    std::printf("%s\n", failures ? "HOTKEY TESTS FAILED" : "ALL HOTKEY TESTS PASSED");
    return failures ? 1 : 0;
}
