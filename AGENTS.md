# AGENTS.md — Dragon Ball Z: Infinite World native port

Static recompilation of **Dragon Ball Z: Infinite World** (PAL SLES_553.47) to a native Linux and Windows executable with
**PS2Recomp** (MIPS R5900 → C++), Metrowerks CodeWarrior 2.4.1.01 target, Vulkan paraLLEl-GS renderer. Built with a native
first-launch setup pipeline (`app/setup/`) that verifies the player's retail disc and extracts required assets to local
appdata. Goal: full game playable natively at 60 FPS (PAL60) and 50 FPS (PAL50), 4K rendering, widescreen support,
instant loading, sub-frame input response (gamepad/fightstick), CRI ADX audio and Sofdec SFD cutscene playback, and zero
emulation overhead. GitHub: TheSlackbringer/dbziw-port.

Read first: this file → `notes/issues/README.md` (local: index, "Waiting for you", working agreement) → the current
issue's `spec.md` and `work.md`. Setup/build commands: "Essentials" in this file. Look-ups (repo map, debug switches,
Ghidra, PCSX2, record/replay, pitfalls): "Reference" at the end of this file.

## Hard rules (agreed 2026-10-19 — after an agent weakened tests to make them pass)
- **Never change a test's pass criteria** (thresholds, tick windows, run mode) to make it pass. Changing criteria is
  a spec change: you approve it first. A failing test is reported as failing.
- **No timing workarounds:** no delays, sleeps or knobs tuned to one game routine or one machine. Fix the root cause.
- **Audit stubs before binding:** never bind or override a runtime SDK HLE stub without auditing first (`tools/audit_stubs.py`).
  Verify register ABI ($a0–$a3 vs $t0–$t3, quadword returns) and confirm whether the game's own compiled SDK code can run unhooked.
- **Every claimed result names its log** (`notes/issues/tests/out/...`). "Passes" means the run has finished.
- **Only you approve a spec.** Write "approved" only after you said so, with the date.
- **Pushing is yours:** push only after you agreed to that push.
- Changes to `notes/` are tracked locally; keep your findings, logs, and milestone history updated there.
- Before relying on the game files, verify them: `game/SLES_553.47` must match the player's legally dumped retail disc (PAL v1.00).

## How we work together (agreed 2026-10-16)
1. **One folder per issue**, `notes/issues/NNN-short-name/` (local, never committed; templates in `notes/issues/`):
   - `spec.md` — what we agreed. Copy `notes/issues/TEMPLATE-spec.md` and fill EVERY section:
     what you see / reported symptoms; background (facts with numbers, what is ruled out); how to reproduce; **recommended fix**
     (step by step, why it works, expected result); **native vs emulation** (a header line plus a section: which
     parts are native code, which stay emulated, anything moving between them); **main risks** (what could go wrong
     or need your decision, and what then); alternatives considered (and why not); measurable pass criteria;
     the automated test; the 1–3 "Maintainer test" items only you can judge; out of scope. Plain words, no
     unexplained jargon.
   - `work.md` — stop/go milestones (not in the spec), all your findings, progress, measurements, conclusion, fix
     (commit) and history, kept current. Findings never live only in chat.
2. **Spec approval before any work.** Draft the spec (fact-finding needed to write it is fine); you approve.
   Push back with reasons when you think a spec is wrong; you decide and can overrule. No fixes or experimental
   code changes without an approved spec. While away, you can allow work on an unapproved spec — explicitly.
3. **A test per issue, written first:** `notes/issues/tests/issue_NNN_*.py` prints PASS/FAIL and must FAIL on the bug
   before the fix (where measurable). Runner: `notes/issues/tests/run.py NNN|quick|standard|full|list`; helpers and routes in
   `notes/issues/tests/dbztest.py`. Closed issues keep their tests as regression guards.
   - **Automated visual baseline diffs:** for rendering bugs, use `tools/diff_frames.py <port_frame> <pcsx2_frame>` to compute
     perceptual hash and pixel-diff heatmaps against headless PCSX2 software-renderer baseline captures. Report numerical
     mismatch metrics in the test output rather than relying on subjective eyeballing.
4. **Test tiers (agreed 2026-10-19):**
   - `run.py quick` (~5–10 min): after every change. May use **deterministic boot fast-forwarding or memory checkpoints**
     (`PS2X_LOAD_STATE=<state>` or fast-boot flags) to bypass the initial 400–800 tick boot/warning/menu loops and reach the
     test scene in seconds.
   - `run.py standard` (~30 min): every test once at full length from a cold boot before a "Fix #" commit.
   - `run.py full` (= `all`, ~60 min): every repeat from cold boot before maintainer test sessions and pushes.
   - **Maintainer test sessions:** `run.py full` must pass first. Batch several "fixed — maintainer test" issues into one session
     with one short list. Test window: `run.py window NNN ...` (full logging, log filed under the issues). Normal play: `run.py play`.
     You only check the specs' "Maintainer test" items; record results in `work.md` + status.
5. **Commits and pushes:** one local commit per fixed issue ("Fix #00N: …") when it reaches "fixed — maintainer test".
   Push only after you confirmed the issues in the batch and agreed to the push. Other commits only on command.
6. **Stop/go:** split big work into milestones in `work.md`. When you are present: a two-line status per milestone, and an
   immediate report (with the alternative) when an approach doesn't pay off. When you are away: keep going, log milestones.
7. **While you are away** you may: work on approved specs in the agreed order; run automated tests off-screen;
   update `work.md` and the index; make local per-issue commits; capture PCSX2 references (headless); create NEW
   issues (draft spec + findings) — but not fix them. Wait for you for: fixes without an approved spec, spec
   changes, pushing, any visible window or desktop action, deleting outside your scratch space, downloads/installs/
   system settings, turning the PC off (only when asked that time). Stuck: write it under "Waiting for you" and
   continue with the next approved issue.

## Rules
- **NEVER ASSUME — ask, with a recommendation.** Need something opened, switched, installed or done
  (a Ghidra tab, a PCSX2 screen, a file), or unsure what you have or want: ask what, why, and what you recommend.
- **Decisions are yours:** scope, requirements, priorities, library/function/graphics behaviour choices,
  native-vs-emulation direction. Present 2–4 options with a recommendation; research and diagnosis may continue while
  waiting, applying the choice may not. A request to fix something is not agreement with a particular approach.
- **Native over emulation;** model hardware only where the runtime must. Prefer the game's own Dimps engine / SDK code over runtime
  HLE stubs. Find root causes; no workarounds that paper over symptoms. Survey existing approaches before inventing.
- **Audit stubs before hooking:** verify against the game's compiled code with `tools/audit_stubs.py`. Unbind inaccurate
  HLE stubs so the game's real SDK code executes whenever hardware MMIO is not directly involved.
- **Performance changes must give identical output** (same frames, same sound, exact frame advantage & cancel timing in combat).
  Test at **1x**: every run passes `PS2X_GS_SCALE=1 PS2X_GS_SSAA=1` and you check the `[output] scale=1x` log line.
- **GPU renderer only** (Vulkan/paraLLEl-GS) for development and tests; no CPU-renderer work unless asked.
- **The port is not specific to your hardware.** Keep fixes portable across supported hardware and the
  planned Linux/Windows targets. Your CPU/GPU are test equipment, not product requirements. Label
  performance measurements with the test hardware; do not generalize one machine's timings or introduce
  machine-specific fixes. Correctness is defined by PS2 behaviour.
- **Your desktop is off limits:** never send synthetic input to it, never screenshot it (use frame dumps),
  never unlock it, never edit `~/.config/PCSX2`. Visible windows only for a maintainer test session or when asked.
- **One game at a time;** no builds or test runs while you have a game window open. Tests use the silent sink
  `dbziw_codex_tests`; live play uses the default output. Never change audio defaults or route other apps.
- **Downloads need explicit permission** (name, source, size).
- **Nothing derived from the game in Git:** no game files (`game/`, `discs/`), Ghidra DBs, generated or lifted C++
  (`recomp/output/`, `recomp/vu1/`), frame dumps, captures, AFS/SFD audio/video streams, microcode `.bin`, screenshots.
  Scan before every commit/push. Hooks in `.githooks/` (`git config core.hooksPath .githooks`); never bypass them (`--no-verify`).
- **Releases & native first launch:** follow the proven DW5 architecture (`app/setup/`: `disc_setup`, `setup_pipeline`,
  `wizard_ui`, `wizard_console`). A release contains only our code and freely licensed tools (the builder wizard,
  recompiler, sources, compiler) — never the compiled game, disc files, an ISO, or generated C++ (PS2Recomp is GPL-3.0).
  On first launch, the native setup wizard extracts and verifies the player's retail disc (`SLES_553.47`), prepares local
  directories (`%LOCALAPPDATA%\DBZIW` on Windows, `~/.local/share/dbziw` on Linux), and compiles locally. Releases are
  published only by the maintainer.
- **Commit identity:** author/committer `TheSlackbringer <15327900+TheSlackbringer@users.noreply.github.com>`;
  end messages with `Co-Authored-By: Claude <model> <noreply@anthropic.com>`. Call our renderer fixes "runtime
  patches" (`vendor/`). `external/PS2Recomp/` is vendored: runtime changes are ordinary commits here.
- **Use the Ghidra MCP whenever you can** (`mcp__ghidra__*` tools: decompile, xrefs, strings, renames/comments)
  for reverse engineering; call `get_program_info` first. Headless only when the MCP is unreachable (GUI closed),
  for batch jobs, or for parallel agents — always on a copy (see Reference → Ghidra). If the needed program is not
  the active tab, ask to switch it.
- **Parallel agents** get self-contained briefs and do analysis/patch-writing in their own worktree or scratch dir
  (Ghidra on their own copy); one integrator builds and runs the game. Each worktree gets its own `app/build-<task>`.
- **Never delete build dirs containing `_deps`;** no `rm` with bare shell variables (use `"${VAR:?}"`).
- Keep `AGENTS.md` current when rules or the way of working change. Check usage
  before expensive batches; at ≤10 % left, checkpoint (`work.md`, index) before continuing.

## Essentials
- Build: `cmake --build app/build` (all), `--target dbziw` (game), `--target dbziw_setup` (setup wizard).
  Regenerate code: `tools/regen.sh` (needs the player's disc in `game/`). CTests: `cd app/build && ctest`.
- Audit stubs: `tools/audit_stubs.py game/SLES_553.47 recomp/dbz_iw.toml`.
- Run tests: `notes/issues/tests/run.py quick` (after changes) / `standard` (before a Fix commit) / `full`.
- Visual diffs: `tools/diff_frames.py <port_frame.png> <pcsx2_ref.png> [--threshold 0.02]`.
- Unsure how the original looks or behaves: compare with PCSX2 first (`tools/pcsx2_reference.py`), then ask you
  for what screenshots can't show (feel, combat input response, sound timing).
- Debug code: `DBZIW-DEBUG` (kept, env-gated) and `TEMP-DEBUG` (remove before committing).

## Reference

### Repository map
- `external/PS2Recomp/` — vendored upstream runtime/recompiler plus our runtime fixes (ordinary files, architecture
  reused from `dw5-ps2`).
  - `ps2xRuntime/` — EE runtime: memory, DMA, VIF, VU0/VU1 (interpreter `src/lib/vu/ps2_vu1_core.cpp`, lifted-code
    support `include/runtime/ps2_vu1_lift.h`), GS front end + Vulkan paraLLEl-GS backend, kernel/syscalls, SDK HLE
    stubs (`src/lib/Kernel/Stubs/`), EE scheduler (`src/lib/Kernel/EeScheduler.cpp`), async device queue
    (`src/lib/ps2_device_queue.cpp`), GIF arbiter + GS worker (`src/lib/gs/ps2_gif_arbiter.cpp`), stall watchdog &
    symbolized traceback handler.
  - `ps2xIOP/` — IOP R3000 interpreter running the game's real IRX modules (`CRI_ADXI.IRX`, `LIBSD.IRX`, `PADMAN.IRX`,
    `SIO2MAN.IRX`, `MCMAN.IRX`, `MCSERV.IRX`, `MODMIDI.IRX`, `AMSOUND.IRX`, `MODHSYN.IRX`, `SDRDRV.IRX`, `MODSEIN.IRX`,
    `MODSESQ.IRX`), IOP kernel `src/emulator/core/iop_kernel.cpp`, SPU2 model `src/emulator/core/iop_spu2.cpp`.
  - `ps2xRecomp/` — the recompiler `ps2_recomp` (VU0 macro ops: `src/lib/vu_translator.cpp`).
- `recomp/` — recompiler configuration (`dbz_iw.toml`: entry point `0x100008`, stubs, `[mmio]` map, jump tables,
  `[patches]`), `ghidra_functions.csv` (symbol boundaries across `0x100000`–`0x49C600`), and `recomp/output/`
  (generated C++, gitignored; `tools/regen.sh`).
- `app/` — our executable and native setup pipeline:
  - `src/main.cpp` — host entry point, lifecycle, and runtime initialization.
  - `src/dbziw_pad.cpp` — pad bridge for XInput, DualSense, fightsticks with sub-frame response.
  - `src/dbziw_cri.cpp` — CRI ADX/Sofdec EE buffer management & audio streaming hooks.
  - `src/dbziw_video.cpp` — native SFD video decode bridge.
  - `setup/` — native first-launch setup wizard (`disc_setup`, `setup_pipeline`, `wizard_ui`, `wizard_console`).
  - tests in `app/tests/` (CTest).
- `tools/` — `regen.sh`, `audit_stubs.py`, `diff_frames.py`, `pcsx2_reference.py`, `ghidra_scripts/`, debugging tools.
- Local, gitignored: `game/` (extracted PAL disc: `SLES_553.47`, `SYSTEM.CNF`, `MODULES/`, `USR/`, `mc0/`, `mc1/`
  memory cards), `discs/`, `ghidra/` (project `dbziw`), `notes/` (issues, evidence, history).

### How our code reaches the game
Generated C++ registers every game function under its PS2 address; calls go through that table. Overrides in `app/src`
register per disc serial (`PS2_REGISTER_GAME_OVERRIDE(..., "SLES_553.47", ...)`), verify the expected instruction,
then `runtime.replaceFunction(addr, fn)`; `runtime.lookupFunction(addr)` returns the original so a replacement can wrap
it. No mid-function hooks. Never patch disc files or generated output by hand.

### Running the game by hand (prefer the issue tests and helpers)
From `game`: `PS2X_GS_BACKEND=vulkan PS2X_HIDDEN=1 PS2X_GS_SCALE=1 PS2X_GS_SSAA=1 PULSE_SINK=dbziw_codex_tests
PS2X_HOST_INPUT="<tick>:<KEY>:<hold>,..." PS2X_DUMP_DIR=<dir> PS2X_DUMP_EVERY=<n> PS2X_EXIT_TICK=<n> timeout <s>
../app/build/dbziw ./SLES_553.47 > run.log 2>&1`. Ticks are PAL vsyncs (50/s in PAL50 mode, 60/s in PAL60 mode).
Keys: ENTER (Start), TAB (Select), Z (Square / Punch), V (Triangle / Kick), X or Space (Cross / Guard / Evade),
C (Circle / Ki Blast), Q/E (L1/R1 / Transformation & Aura Burst), Left/Right Shift (L2/R2 / Capsule Activate),
Arrows or WASD (D-pad / Left Stick). Leave ~100 ticks between menu presses.
A host-input script disables real keyboard polling for the whole run: never set `PS2X_HOST_INPUT`/`PS2X_INPUT` for
manual play. Silent sink missing after a reboot: `pactl load-module module-null-sink sink_name=dbziw_codex_tests`.
Stop a game with `kill <pid>` (never `pkill -f dbziw` from a shell whose own command line contains that string).
Presented-window timing without touching the desktop: instead of `PS2X_HIDDEN=1`, prefix the game with
`gamescope --backend headless -W 2560 -H 1440 -w 2560 -h 1440 -r 240 --` (your monitor). Frame dumps are the
presented frames (`frame_<tick>.png`); contact sheet:
`magick montage frame_*.png -tile 4x4 -geometry 256x179+2+2 -label '%f' sheet.png`.

### Crash & Stall Diagnostics (Guest Stack Traceback & Stall Watchdog)
- **Stall Watchdog (`PS2X_STALL_WATCHDOG=<s>`):** if the EE thread runs for $>s$ seconds (default: 3s) without
  yielding to a VSync or device poll, a background watchdog thread dumps:
  - Active EE Program Counter (`pc`) and guest function name symbolized via `ghidra_functions.csv`.
  - Symbolized EE call stack traceback.
  - The last 10 SIF DMA packets and IOP RPC function IDs (e.g. CRI ADX streams, audio commands).
  - Active VU1 execution status (`pc`, running microprogram, status flags).
- **Guest Crash Handler (`PS2X_CRASH_HANDLER=1`):** catches guest segmentation faults, bus errors, or unmapped memory
  accesses, printing the exact symbolized guest function and register contents before aborting.

### Debug switches (env, all optional)
Diagnostics/pacing: `PS2X_STALL_WATCHDOG=3` · `PS2X_CRASH_HANDLER=1` · `PS2X_STATS` (vsync/s per second) ·
`PS2X_HUD_LOG` ([hud] fps/vsync/frameMs/maxFrameMs every 0.5 s) · `PS2X_LATE_VBLANK` (fields >3 ms late) ·
`PS2X_VU1_ASYNC_TRACE` (device worker busy/drains per second; "explicit=N" render syncs = 2 x game FPS) ·
`PS2X_VU1_CENSUS=<file>` (per-VU1-program runs/cycles/host ns) · `PS2X_VU1_LIFT=0` (interpret all VU1) ·
`PS2X_VU1_ASYNC=0` (synchronous device) · `PS2X_GS_ASYNC=0` (GS inline, no worker thread) ·
`PS2X_LOAD_STATE=<file>` (resume from memory checkpoint for fast testing) · `PS2X_DUMP_STATE=<file>`.
Graphics: `PS2X_GS_SCALE` / `PS2X_GS_SSAA` · `PS2X_NO_WEAVE` · `PS2X_GS_SYNCREADBACK` · `PS2X_GS_TRACE=<file>` +
`_FROM/_TO` · `PS2X_GS_RECORD=<file>` + `_FROM/_TO` · `PS2X_VRAM_DUMP=<file>` · `PS2X_XGKICK_CAPTURE=<prefix>` + `_FROM/_TO`.
Sound/IOP: `PS2X_SPU2_TRACE` · `PS2X_LIBSD_TRACE` · `PS2X_CRI_TRACE` (CRI ADX/Sofdec stream logging) ·
`PS2X_AUDIO_DUMP=<wav>` (s16 stereo 48 kHz) · `PS2X_AUDIO_TRACE` · `PS2X_SIFAUDIO_TRACE` · `PS2X_IOP_HEAP_TRACE` ·
`PS2X_IO_TRACE`. Heavy IOP tracing starves audio streams: judge audio untraced.
Other: `PS2X_SFD_TRACE` / `PS2X_IPU_TRACE` · `PS2X_PAD_TRACE` · `PS2X_RAM_DUMP_TICKS=t1,t2` · `PS2X_RAM_WATCH=addr:len`.
Debug code in the runtime is tagged `DBZIW-DEBUG`; temporary probes `TEMP-DEBUG` (remove before committing).

### Comparing with the original (PCSX2)
`tools/pcsx2_reference.py --out <scratch> --seconds <n> [--interval 0.5] [--keys "<sec>:<keysym>,..."]` runs
PCSX2 headless on a private Xvfb display with a throwaway copy of your config and the software renderer,
taking F8 screenshots into `<out>/snaps`. Capture the same moment from the port and compare side by side.
Use `tools/diff_frames.py` for automated numerical and visual difference heatmaps. Screenshots are game-derived
(scratch or `notes/evidence/` only). Never touch `~/.config/PCSX2` or send input to the real desktop.

### Rendering bugs: record and replay one frame
`PS2X_GS_RECORD=<file> PS2X_GS_RECORD_FROM=<tick> PS2X_GS_RECORD_TO=<tick>` records every renderer call (starts with
a VRAM snapshot). `app/build/dbziw_gsrender <rec> <cpu|vulkan> <outdir> <ticks> [--fb ...]` replays it; bisect with
`--stop-at`. `dbziw_gsrec dump <rec> --from N --count M --fields` lists the draws. Vulkan renderer fixes are runtime
patches in `vendor/parallel-gs-patches/`.

### Ghidra
- Ghidra 12.1.2 at `/opt/ghidra`. EE language `r5900:LE:32:default` (ghidra-emotionengine-reloaded); IRX modules
  `MIPS:LE:32:default`. GhidraMCP plugin serves the active CodeBrowser tab at `http://127.0.0.1:8080/`. Call
  `get_program_info` first: the GUI may show another program.
- **MCP (interactive, fastest for many small lookups):** Ghidra GUI → File → Open Project `ghidra/dbziw.gpr` →
  double-click `SLES_553.47` → CodeBrowser with GhidraMCP serves `http://127.0.0.1:8080/`. Tools: get_program_info
  (FIRST), decompile_function_by_address, disassemble_function, get_function_by_address, get_xrefs_to/from,
  get_function_calls, list_functions/strings/segments, rename_function_by_address, set_*_comment. Limits: hardware
  registers (0x10000000+, 0x12000000+) and scratchpad (0x70000000+) have no xrefs → use `[mmio]` map in `dbz_iw.toml`.
- ONE source project `ghidra/dbziw.gpr` (renames/comments go here, via GUI/MCP). Headless never runs on it: copy
  `ghidra/dbziw.gpr` + `ghidra/dbziw.rep` to your scratch dir, run
  `analyzeHeadless <dir> dbziw -process SLES_553.47 -noanalysis -readOnly -scriptPath tools/ghidra_scripts -postScript DecompileAt.java 0xADDR ...`,
  then delete the copy.
- Quick look at one function: the generated `recomp/output/*_0x<addr>.cpp` comments every instruction.

### Pitfalls
- **Metrowerks CodeWarrior 2.4.1.01 peculiarities:** Dimps compiled Infinite World with Metrowerks CodeWarrior rather than
  GCC. Pay close attention to jump table layouts (switch tables), epilogue register restores, and function alignment.
- **PAL50 vs PAL60 modes:** Infinite World boots into a 50Hz/60Hz selector screen. PAL60 runs at ~59.94 Hz (60 vsync/s) while
  PAL50 runs at 50 Hz (50 vsync/s). The runtime VSync cadence and IOP timer ratios must adapt to the selected mode.
- **CRI ADX audio & Sofdec SFD video:** Infinite World relies heavily on CRI middleware (`CRI_ADXI.IRX`, AFS archives,
  and SFD video streams). Data transfer from IOP to EE streaming buffers must not starve or hitch. Heavy IOP tracing will
  starve audio streams: evaluate audio untraced.
- **Combat frame cancellation and juggle physics:** Dimps fighter mechanics (cancels, Aura Bursts, fatigue meters) rely on
  exact frame windows. Timing hacks or sleeps will desync combat physics and break combos.
- **Keep `[patches]` in `dbz_iw.toml` empty:** `ps2_analyzer` falsely flags ordinary stores as self-modifying code.
- **Prefer running the game's actual SDK code over inaccurate runtime HLE stubs:** always audit with `tools/audit_stubs.py`.
