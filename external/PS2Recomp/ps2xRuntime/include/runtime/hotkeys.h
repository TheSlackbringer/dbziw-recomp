#pragma once
// DBZIW 013 (DW5's one-table layout, DW5 eaa68f3 / 3c6b453; you, 2026-11-08: "b"): the function keys of the game
// window, in one table so no key gets two jobs. The handlers (ps2_runtime.cpp, ps2_debug_panel.cpp) read their key from
// here; the CTest hotkeys checks the layout, that keys are unique, that no handler names a KEY_Fn itself, and that
// README.md's key table lists the same keys. DW5's F5 / F9 (save and load state) and F10 (Discs window) are not bound:
// DBZIW has no save states yet (issue 016 brings them) and one disc.
#include "raylib.h"

namespace ps2x::hotkeys
{
    enum class Action
    {
        Debugger,     // F1: debugger panel (developer builds, PS2X_DEBUG_UI=1 in the player build)
        FpsOverlay,   // F2: frame rate overlay (was F3)
        AntiAliasing, // F3: anti-aliasing choice, saved in dbziw.ini (was F5)
        OutputScale,  // F4: output resolution 1x / 2x / 4x, saved in dbziw.ini
        BuildOverlay, // F8: build ID and game tick overlay (was F2)
    };

    struct Binding
    {
        Action action;
        int key;          // raylib KeyboardKey
        const char *name; // as written in README.md
    };

    inline constexpr Binding kBindings[] = {
        {Action::Debugger, KEY_F1, "F1"},     {Action::FpsOverlay, KEY_F2, "F2"}, {Action::AntiAliasing, KEY_F3, "F3"},
        {Action::OutputScale, KEY_F4, "F4"},  {Action::BuildOverlay, KEY_F8, "F8"},
    };

    constexpr const Binding &binding(Action action)
    {
        for (const Binding &b : kBindings)
            if (b.action == action)
                return b;
        return kBindings[0]; // unreachable: every Action has a binding (hotkeys_test)
    }
    constexpr int key(Action action) { return binding(action).key; }
    constexpr const char *name(Action action) { return binding(action).name; }
} // namespace ps2x::hotkeys
