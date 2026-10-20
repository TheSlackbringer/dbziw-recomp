<h1 align="center">DBZ: Infinite World Recomp</h1>

<p align="center"><b>Dragon Ball Z: Infinite World (PS2) as a native PC game, built on your PC from your own disc.</b></p>

<p align="center">
  <a href="https://github.com/TheSlackbringer/dbziw-port/actions/workflows/build.yml"><img alt="build" src="https://img.shields.io/github/actions/workflow/status/TheSlackbringer/dbziw-port/build.yml?branch=main&label=build&logo=github"></a>
  <img alt="platform: Windows | Linux" src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux-0a7bbb">
  <img alt="graphics: Vulkan" src="https://img.shields.io/badge/graphics-Vulkan-ac162c">
  <a href="LICENSE"><img alt="license: GPL-3.0" src="https://img.shields.io/badge/license-GPL--3.0-6a9a1f"></a>
  <img alt="status: in development" src="https://img.shields.io/badge/status-in%20development-e6a200">
  <img alt="disc: PAL SLES-55347" src="https://img.shields.io/badge/disc-PAL%20SLES--55347-555">
</p>

A native PC port of **Dragon Ball Z: Infinite World** for the PlayStation 2, built with PS2Recomp. The game's logic is statically translated into C++; the runtime provides the PS2 hardware services it still needs. Graphics use **Vulkan through paraLLEl-GS**.

**In development.** Windows builds come as a self-building release (below); on Linux the game is built from source for now, a Linux release is planned. Full playability has not been established.

> [!IMPORTANT]
> You need your own legal copy of **Dragon Ball Z: Infinite World** (European PAL, `SLES_553.47`). Nothing from the game is in this repository or in its releases: no disc images, game executables, IRX modules, AFS archives, SFD videos, game assets or translated game code. The game is built on your own PC from your own disc.

## Current progress

| Area | State | Details |
| --- | :---: | --- |
| Boot and menus | 🚧 | ELF header parsed, recompiler profiling and entry point initialization in bring-up. |
| 50Hz / 60Hz Mode | 🚧 | In-game PAL60 (59.94 Hz) and PAL50 display rate selection. |
| Input | 🚧 | Keyboard and gamepad/fightstick input (DirectInput, XInput, DualSense). |
| Audio & CRI Middleware | 🚧 | `CRI_ADXI.IRX` streaming, `ADX_*.AFS` music and dual JP/EN voices, SPU2 sound effects. |
| Cutscenes & SFD Video | 🚧 | Sofdec MPEG video playback for `OPENING.SFD`, `ENDING.SFD`, and story cutscenes. |
| Saves | 🚧 | PS2 Memory Card saving and loading (`mc0/`, `mc1/`). |
| Combat & 60 FPS Engine | 🚧 | Dimps fighter engine, frame cancel windows, fatigue gauge, and Aura Burst responsiveness. |
| Dragon Missions / Story | 🚧 | Dragon Missions, minigames (Snake Way, gravity room), and Z-Campaign. |

✅ works in tested scenes · 🚧 in development

Extended session stability and higher-resolution scaling performance remain in active development. These results describe tested scenes, not a complete playthrough or a guarantee that every mode works.

## Supported discs

| Game | Version | Executable | Required? |
| --- | --- | --- | --- |
| Dragon Ball Z: Infinite World | European PAL | `SLES_553.47` | Yes |

Other regions and revisions are unsupported (the program checks every file against known checksums). Verification fails when the disc image is missing, modified, or invalid.

## Playing on Windows

The release is a **builder**, not a game: it contains our program, the source code, a compiler and the tools, and builds the game on your PC from your own disc. You install nothing else — no Visual Studio, no Python, no Git — and nothing is downloaded while it builds.

**You need:** Windows 10 (version 1903 or newer) or 11, 64-bit; a CPU with AVX2 (Intel since 2013, AMD Ryzen); a graphics card with a current Vulkan driver; about 10 GB of free space; your disc image as `.iso`.

1. Download `DBZIW-<version>-windows-x64.zip` from the Releases page and **unzip it close to the drive root**, e.g. `C:\Games\` (the compiler inside has deep folders, and Windows trips over paths longer than 260 characters).
2. Start **`DBZIW.exe`**. The program is not signed, so Windows may say *"Windows protected your PC"*: choose **More info → Run anyway**.
3. **First start:** choose your DBZ: Infinite World disc image (Browse, or drag it onto the window) and press **Start**. The disc is checked and copied, the game's code is translated and compiled for your PC — about **5–10 minutes** once. Then the game starts.
4. **Every later start:** `DBZIW.exe` starts the game at once. A newer release rebuilds once, without asking for the disc again.

Your game files, the built game, saves, settings and logs are in `%LOCALAPPDATA%\DBZIW`. Replacing the release folder with a newer version keeps them. If something goes wrong, the logs are in `%LOCALAPPDATA%\DBZIW\logs` (`setup.log`, `build.log`, `game.log`); please attach them to an issue.

## Playing on Linux

A Linux release (an AppImage with the same self-building wizard) is planned. Until then, build from source as described below; `dbziw_setup` (built with the game) offers the same wizard window for building from your own disc.

## Building on Linux

Run the following from the repository root. You need an x86-64 CPU supporting x86-64-v3, Clang, lld, CMake 3.21 or newer, Ninja, Git, rsync, pkg-config, Vulkan drivers, and development packages for FFmpeg, the window system and audio. FFmpeg must provide `libavcodec`, `libavformat`, `libavutil`, `libswresample` and `libswscale`. CMake may download other build dependencies during configuration.

### 1. Prepare your disc files

Extract your retail PAL disc into `game/`, preserving its directory structure. For example, using `bsdtar`:

```sh
mkdir -p game
bsdtar -xf "/path/to/your/dbz-infinite-world.iso" -C game
```

`SLES_553.47`, `SYSTEM.CNF`, `MODULES/`, and `USR/` must be in that directory. Keep all extracted assets; the executable alone is insufficient.

### 2. Generate the native game code

```sh
CC=clang CXX=clang++ cmake -S external/PS2Recomp -B external/PS2Recomp/build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPS2X_BUILD_RUNTIME=OFF -DPS2X_BUILD_ANALYZER=OFF \
  -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF
cmake --build external/PS2Recomp/build --target ps2_recomp
tools/regen.sh all
```

This generates the main executable translations locally. Ghidra is used for reverse engineering; the existing function maps support this generation step without creating a new Ghidra project.

### 3. Prepare the Vulkan renderer

The wrapper expects the pinned paraLLEl-GS revision below and its required submodules:

```sh
git clone https://github.com/Arntzen-Software/parallel-gs.git app/deps/parallel-gs
git -C app/deps/parallel-gs checkout cc6184af7e0c03da603045ca371ffa5dae9b0655
git -C app/deps/parallel-gs submodule update --init Granite
git -C app/deps/parallel-gs/Granite submodule update --init \
  third_party/volk third_party/khronos/vulkan-headers
```

Then apply this project's renderer patches (internal resolution scaling and anti-aliasing):

```sh
git -C app/deps/parallel-gs apply "$PWD"/vendor/parallel-gs-patches/*.patch
```

If that checkout already exists, reuse it after verifying the revision.

### 4. Build the application

```sh
CC=clang CXX=clang++ cmake -S app -B app/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_LINKER_TYPE=LLD \
  -DPS2X_ENABLE_VULKAN_GS=ON \
  -DPS2X_PARALLEL_GS_SOURCE_DIR="$PWD/app/deps/parallel-gs" \
  -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF
cmake --build app/build --target dbziw
```

The verbose per-function logging option is disabled here because it substantially slows gameplay.

## Running and controls

```sh
cd game
PS2X_GS_BACKEND=vulkan ../app/build/dbziw ./SLES_553.47
```

In this developer setup, saves are stored in `game/mc0/`. Keep that directory when updating your build. (The Windows release keeps everything in `%LOCALAPPDATA%\DBZIW` instead.)

### Controls

| Keyboard | Gamepad / Fightstick | PS2 input | Action |
| --- | --- | --- | --- |
| Enter | Start | Start | Pause / Start |
| Tab | Select | Select | Select / Taunt |
| Z | Square (X) | Square | Punch |
| V | Triangle (Y) | Triangle | Kick |
| X or Space | Cross (A) | Cross | Guard / Evade / Cancel |
| C | Circle (B) | Circle | Ki Blast |
| Q | L1 (LB) | L1 | Aura Burst / Dragon Dash |
| E | R1 (RB) | R1 | Skill / Capsule Select |
| Left Shift | L2 (LT) | L2 | Transformation / Charge |
| Right Shift | R2 (RT) | R2 | Capsule Activate |
| Arrow keys or WASD | D-pad / Left Stick | D-pad | Movement |

Gamepads and arcade fightsticks are supported out of the box.

### Function keys and display

| Key | Description |
| --- | --- |
| **F1** | Runtime debugger (developer builds; off in release unless `PS2X_DEBUG_UI=1`). |
| **F2** | Toggle build ID and game tick HUD overlay (useful for bug reports and log alignment). |
| **F3** | Toggle real game FPS, VSync rate, resolution scale, and AA sample HUD counter. |
| **F4** | Cycle internal resolution scale live ($1\times \to 2\times \to 4\times$) and persist to `dbziw.ini`. |
| **F5** | Cycle anti-aliasing sample multiplier ($1\times \to 2\times \to 4\times \to 8\times \to 16\times$) and persist to `dbziw.ini`. |
| **F9** | Discs window (release build): disc verification and file diagnostics. |

You can also pass environment variables on startup: `PS2X_FPS=1` enables the F3 overlay by default, `PS2X_GS_SCALE=1|2|4` sets the startup resolution scale, and `PS2X_GS_SSAA=1|2|4|8|16` sets the anti-aliasing sample count (defaults can be kept at `1` when profiling performance).

## Development

Fixes live in handwritten app/runtime code, recompiler profiles and generation tools. Original disc files remain unchanged; generated translations are regenerated rather than edited by hand.

| Path | Contents |
| --- | --- |
| `app/` | Application, native bridges, controller input, and regression tests |
| `recomp/` | PAL profiles (`dbz_iw.toml`), function maps (`ghidra_functions.csv`), generated output |
| `tools/` | Code generation (`regen.sh`), Ghidra scripts, PCSX2 reference runner, debugging tools |
| `external/PS2Recomp/` | Vendored runtime and recompiler with maintained modifications |
| `vendor/runtime-patches/` | Runtime patch provenance |
| `licenses/` | Third-party notices |

Game data, analysis databases, generated output, builds and local research notes are excluded from Git. Contributor and coding-agent workflows are described in `AGENTS.md` and `CLAUDE.md`.

Current priorities are entry point bring-up, PAL60/PAL50 cadence handling, CRI ADX/Sofdec playback, and initial battle rendering.

### Making a release

Releases contain only our code and freely licensed tools — never the compiled game, disc files, an ISO or translated code (PS2Recomp is GPL-3.0; see `AGENTS.md`). On a clean commit, after the full test suite:

```sh
cmake --build app/build-release-win --target dbziw_setup     # the launcher, cross-compiled with llvm-mingw
cmake --build build-recomp-win --target ps2_recomp          # the recompiler for Windows
tools/release/make_release.py windows --build app/build-release-win --recomp-build build-recomp-win
```

The script refuses a dirty tree, packs the launcher, sources, compiler (llvm-mingw for Windows), CMake, Ninja, the minimal FFmpeg and the licenses, scans the package for anything from the game, and writes the zip and its SHA-256 to `dist/`. Publishing is done by the maintainer.

## License and attribution

The port's own source and tools are licensed under GPL-3.0; see [LICENSE](LICENSE). This license grants no rights to the original game or its assets.

The vendored [PS2Recomp](https://github.com/ran-j/PS2Recomp) runtime and recompiler are modified versions of the upstream project by ran-j and contributors. Third-party code retains its applicable licenses; see `THIRD_PARTY.md` and accompanying notices.

Dragon Ball, Dragon Ball Z, and related characters are trademarks and copyrights of Bird Studio / Shueisha / Toei Animation. Dragon Ball Z: Infinite World is developed by Dimps and published by Atari / Bandai Namco Games. This non-profit fan preservation project is not affiliated with, endorsed by, or sponsored by Dimps, Atari, Bandai Namco Games, Bird Studio, Shueisha, or Toei Animation.
