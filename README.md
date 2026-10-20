# Dragon Ball Z: Infinite World — Native PC Port (dbziw-port)

A native Windows/Linux static recompilation port of **Dragon Ball Z: Infinite World** (PlayStation 2, PAL `SLES-55347`) using `ps2recomp`.

---

## 🎯 Project Goals
* **True Native Execution:** Ahead-of-time recompilation of MIPS R5900 Emotion Engine & VU0/VU1 microcode to portable C++.
* **Modern PC Enhancements:**
  * Clean 60 FPS tick rate using the game's native PAL60 display mode.
  * Native 4K rendering and widescreen presentation via Vulkan paraLLEl-GS.
  * Modern controller support (XInput, DualSense, arcade fightsticks) with sub-frame response.
* **Architecture Synergy:** Reuses the verified `ps2recomp` runtime and Vulkan Graphics Synthesizer backend from `dw5-ps2`.

---

## 🏗️ Project Architecture

```
dbziw-port/
├── game/                   # Extracted game files from player's disc (gitignored)
│   ├── SLES_553.47         # Uncompressed base game executable (PAL v1.00)
│   ├── SYSTEM.CNF          # Boot config
│   ├── MODULES/            # IOP sound/controller driver IRX modules
│   └── USR/                # CRI AFS sound/data archives and SFD movies
├── recomp/                 # Recompiler configuration & generated C++
│   ├── dbz_iw.toml         # Entry point, MMIO mapping, stubs
│   ├── ghidra_functions.csv# Symbol boundaries
│   └── output/             # C++ source translated by ps2_recomp (gitignored)
├── tools/                  # Code generation scripts and Ghidra tooling
├── port/                   # Native runtime environment & host application
│   ├── CMakeLists.txt
│   └── src/                # Game-specific overrides and input bridges
└── notes/                  # Hardware documentation, memory maps, and progress logs
```

---

## ⚖️ Legal & Preservation Policy
This repository contains **NO copyrighted game assets, ISOs, ELFs, textures, or audio**. 
All recompiled C++ is generated locally on the developer's machine from their own dumped retail disc.
