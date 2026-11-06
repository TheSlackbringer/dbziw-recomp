# Building and developing DBZ: Infinite World Recomp

Players do not need this: the [releases](https://github.com/TheSlackbringer/dbziw-recomp/releases) build the game for you (see the [README](README.md)). This page is for building from source on Linux, the developer tools, and making a release.

## Building on Linux

Run the following from the repository root. You need an x86-64 CPU supporting x86-64-v3, Clang, lld, CMake 3.21 or newer, Ninja, Git, rsync, pkg-config, Vulkan drivers, and development packages for FFmpeg, the window system and audio. FFmpeg must provide `libavcodec`, `libavformat`, `libavutil`, `libswresample` and `libswscale`. CMake may download other build dependencies during configuration.

### 1. Prepare your disc files

Extract your retail PAL disc into `game/`, preserving its directory structure. For example, using `bsdtar`:

```sh
mkdir -p game
bsdtar -xf "/path/to/your/dbz-infinite-world.iso" -C game
```

`SLES_553.47`, `SYSTEM.CNF`, `MODULES/` and `USR/` must be in that directory. Keep all extracted assets; the executable alone is insufficient.

### 2. Generate the native game code

```sh
CC=clang CXX=clang++ cmake -S external/PS2Recomp -B external/PS2Recomp/build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPS2X_BUILD_RUNTIME=OFF -DPS2X_BUILD_ANALYZER=OFF \
  -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF
cmake --build external/PS2Recomp/build --target ps2_recomp
tools/regen.sh
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

Then apply this project's renderer patches (internal resolution scaling and anti-aliasing), kept in `vendor/parallel-gs-patches/`:

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

The verbose per-function logging option is disabled here because it substantially slows gameplay. `dbziw_setup` (built with the game) offers the release's wizard window for building from your own disc.

### 5. Run

```sh
cd game
PS2X_GS_BACKEND=vulkan ../app/build/dbziw ./SLES_553.47
```

In this developer setup, saves are stored in `game/mc0/` and settings in `game/dbziw.ini`. Keep them when updating your build.

## Developer switches

Environment variables on startup: `PS2X_FPS=1` turns the F3 overlay on, `PS2X_GS_SCALE=1|2|4` sets the startup resolution scale and `PS2X_GS_SSAA=1|2|4|8|16` the anti-aliasing sample count (keep scale and anti-aliasing at `1` when profiling performance). `AGENTS.md` lists the rest.

## Development

Fixes live in handwritten app/runtime code, recompiler profiles and generation tools. Original disc files remain unchanged; generated translations are regenerated rather than edited by hand.

| Path | Contents |
| --- | --- |
| `app/` | Application, setup launcher, native bridges and regression tests |
| `recomp/` | PAL profile (`dbz_iw.toml`), function map (`ghidra_functions.csv`), generated output |
| `tools/` | Code generation (`regen.sh`), Ghidra scripts, PCSX2 reference runner, debugging and release tools |
| `external/PS2Recomp/` | Vendored runtime and recompiler with maintained modifications |
| `vendor/runtime-patches/` | Runtime patch provenance |
| `licenses/` | Third-party notices |

Game data, analysis databases, generated output, builds and local research notes are excluded from Git. Contributor and coding-agent workflows are described in `AGENTS.md` and `CLAUDE.md`.

Debugging tools:

- [Compare saved frames exactly](tools/debug/compare_frames.md): changed-pixel counts, locations and an optional difference image.
- [Validate function maps and generated output](tools/debug/validate_function_map.md): boundary, path and emission evidence for map/export and missing-instruction investigations, without changing maps or running the game.
- [Audit SDK bindings](tools/debug/audit_bindings.md): evidence for a suspected SDK replacement error.

## Making a release

Releases contain only our code and freely licensed tools — never the compiled game, disc files, an ISO or translated code (PS2Recomp is GPL-3.0; see `AGENTS.md`). On a clean commit, after the full test suite:

```sh
cmake --build app/build-release-win --target dbziw_setup dbziw_bootstrap dbziw_uninstall  # the launcher, cross-compiled with llvm-mingw
cmake --build build-recomp-win --target ps2_recomp                                        # the recompiler for Windows
tools/release/make_release.py windows --build app/build-release-win --recomp-build build-recomp-win
```

The script refuses a dirty tree, packs the launcher, sources, compiler (llvm-mingw for Windows), CMake, Ninja, the minimal FFmpeg (`tools/release/build_ffmpeg.sh`) and the licenses, scans the package for anything from the game, and writes the zip and its SHA-256 to `dist/`. The Linux AppImage is built in a pinned container; see [tools/release/LINUX.md](tools/release/LINUX.md). Publishing is done by the maintainer.
