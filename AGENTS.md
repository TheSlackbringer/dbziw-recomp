# AGENTS.md — Dragon Ball Z: Infinite World native port

Static recompilation of **Dragon Ball Z: Infinite World** (PAL SLES_553.47) to a native Linux and Windows executable with
**PS2Recomp** (MIPS R5900 → C++), Metrowerks CodeWarrior 2.4.1.01 target, Vulkan paraLLEl-GS renderer. Built with a native
first-launch setup pipeline (`app/setup/`) that verifies the player's retail disc and extracts required assets to local
appdata. Goal: full game playable natively at 60 FPS (PAL60) and 50 FPS (PAL50), 4K rendering, widescreen support,
instant loading, sub-frame input response (gamepad/fightstick), CRI ADX audio and Sofdec SFD cutscene playback, and zero
emulation overhead. GitHub: TheSlackbringer/dbziw-recomp.

Read first: this file → `notes/issues/README.md` (local: index, "Waiting for you", working agreement; the line under
its title points to the private `notes/AGENTS.md`) → `notes/issues/ORDER.md` (the agreed order and the DBZIW ↔ DW5
number key) → the current issue's `spec.md` and `work.md`. Setup/build commands: "Essentials" in this file. Look-ups
(repo map, debug switches, Ghidra, PCSX2, record/replay, pitfalls): "Reference" at the end of this file. Lineage: the
runtime and way of working come from `../dw5-ps2` (forked at about DW5 `438a3de`, 2026-10-20); later DW5 work arrives
through the issues in `ORDER.md`'s number key (DW5 issues are always written "DW5 NNN"; the two series overlap).

Items marked *(planned: NNN)* below do not exist yet; that issue brings them.

## Hard rules (2026-10-19 — after an agent weakened tests to make them pass)
- **Never change a test's pass criteria** (thresholds, tick windows, run mode) to make it pass. Changing criteria is
  a spec change: the user approves it first. A failing test is reported as failing.
- **No timing workarounds:** no delays, sleeps or knobs tuned to one game routine or one machine. Fix the root cause.
- **Evidence before binding:** never bind, unbind or override a runtime SDK HLE stub without evidence of the guest
  contract (arguments, returns, memory written, callbacks, state owned) and whether the game's own compiled SDK code
  can run unhooked. `tools/audit_stubs.py` gives candidates, not proof (issue 024 builds the evidence report).
- **Every claimed result names its log** (`notes/issues/tests/out/...`). "Passes" means the run has finished.
- **Only the user approves a spec.** Write "approved" only after the user said so, with the date.
- **Never commit or close an issue without explicit textual approval (DBZIW rule, stricter than DW5):** NEVER create
  a `Fix #00N` commit and NEVER mark an issue `confirmed` or `closed` until the user explicitly types "confirmed",
  "approved", or "close" in chat. Reaching `fixed — maintainer test` means STOP and wait for review.
  An image upload, terminal paste, test run, or empty message is NEVER approval to commit or close.
- **Never ask the user to run an automated test:** automated tests are exclusively the agent's job to
  run, verify, and log. The "Maintainer test" section is strictly reserved for things a machine cannot
  judge (gameplay feel, controller responsiveness, visual artifacts, sound timing). If an issue is
  100% covered by automated tests, the Maintainer test is explicitly "None (fully automated)".
- **Pushing requires permission:** push only after the user agreed to that push.
- Changes to `notes/` are tracked in its own local git repo (`git -C notes log`); commit your notes changes there.
- Before relying on the game files, verify them: `game/SLES_553.47` must match the player's legally dumped retail disc (PAL v1.00)
  (a checksum check like DW5's `notes/tools/check_game_files.sh`: *planned: 017*).

## Working agreement (2026-10-16)
1. **One folder per issue**, `notes/issues/NNN-short-name/` (local, never committed; templates in `notes/issues/`):
   - `spec.md` — what the user and you agreed. Copy `notes/issues/TEMPLATE-spec.md` and fill EVERY section:
     what the user sees; background (facts with numbers, what is ruled out); how to reproduce; **recommended fix**
     (step by step, why it works, expected result); **native vs emulation** (a header line plus a section: which
     parts are native code, which stay emulated, anything moving between them); **main risks** (what could go wrong
     or need a user decision, and what then); alternatives considered (and why not); measurable pass criteria;
     the automated test; the 1–3 "Maintainer test" items that require human judgement; out of scope. Plain words, no
     unexplained jargon. When a spec adopts DW5 work, cite real DW5 commits and paths and check them
     (`git -C ../dw5-ps2 log -1 <sha>`; is it before or after the fork?).
   - `work.md` — stop/go milestones (not in the spec), all your findings, progress, measurements, conclusion, fix
     (commit) and history, kept current. Findings never live only in chat.
   - **Order and ownership (user, 2026-10-27):** each spec appears once in `notes/issues/ORDER.md` and is done whole;
     a part that must happen at another time becomes a new spec first (the one exception is written in ORDER.md).
     Each test, tool or change has one owning spec; other specs use it and never port it again.
2. **Spec approval before any work.** Draft the spec (fact-finding needed to write it is fine); the user approves.
   Push back with reasons when you think a spec is wrong; the user decides and can overrule. No fixes or experimental
   code changes without an approved spec. While away, the user can allow work on an unapproved spec — explicitly.
3. **A test per issue, written first:** `notes/issues/tests/issue_NNN_*.py` prints PASS/FAIL and must FAIL on the bug
   before the fix (where measurable). Runner: `notes/issues/tests/run.py NNN|quick|standard|full|list`; helpers and routes in
   `notes/issues/tests/dbztest.py` *(planned: 017)*. Closed issues keep their tests as regression guards.
   - **Automated visual baseline diffs:** for rendering bugs, use `tools/diff_frames.py <port_frame> <pcsx2_frame>` to compute
     perceptual hash and pixel-diff heatmaps against headless PCSX2 software-renderer baseline captures. Report numerical
     mismatch metrics in the test output rather than relying on subjective eyeballing.
4. **Testing by change (2026-10-21 in DW5, adopted 2026-10-26; replaces the mandatory suite gates):** run all fast
   automated checks after every change: unit tests (CTest), Python unit tests and static checks. Also run checks
   relevant to the changed code and behaviour. Use CI to validate builds *(planned: 007)*. Slow gameplay runs are
   separate from the fast checks; they may resume from memory checkpoints (`PS2X_LOAD_STATE=<state>`) to skip the
   400–800 tick boot. **After fixing at least three specs, recommend a full-suite run and ask the user directly before
   running it.** Never start the full suite without the user's explicit approval or request. There is no automatic
   broad-suite requirement before commits, pushes or maintainer test sessions. Test thresholds, tick windows, repeats
   and pass criteria stay unchanged; report failures and incomplete runs honestly, with logs. Batch several
   "fixed — maintainer test" issues into one session with one short list; the user only checks the specs'
   "Maintainer test" items; record results in `work.md` + status. Test window: `run.py window NNN ...` (full logging
   filed under the issues). Normal play: `run.py play`.
5. **Commits and pushes:** one local commit per fixed issue ("Fix #00N: …") ONLY AFTER the user explicitly confirms the
   maintainer test ("confirmed" / "close"). Never commit upon merely setting status to "fixed — maintainer test". Push
   only after the user confirmed the issues in the batch and agreed to the push. Other commits only on command.
6. **Stop/go:** split big work into milestones in `work.md`. User present: a two-line status per milestone, and an
   immediate report (with the alternative) when an approach doesn't pay off. User away: keep going, log milestones.
7. **While the user is away** you may: work on approved specs in the agreed order (`notes/issues/ORDER.md`); run
   automated tests off-screen; update `work.md` and the index; prepare fixes up to "fixed — maintainer test" (commit
   "Fix #00N" only with explicit prior permission); capture PCSX2 references (headless); create NEW issues (draft spec
   + findings) — but not fix them. Wait for the user for: fixes without an approved spec, spec changes, pushing, any
   visible window or desktop action, deleting outside your scratch space, downloads/installs/system settings, turning
   the PC off (only when asked that time). Stuck: write it under "Waiting for you" and continue with the next approved
   issue.

## Rules
- **NEVER ASSUME — ask the user, with a recommendation.** Need something opened, switched, installed or done
  (a Ghidra tab, a PCSX2 screen, a file), or unsure what the user has or wants: ask what, why, and what you recommend.
- **Decisions belong to the user:** scope, requirements, priorities, library/function/graphics behaviour choices,
  native-vs-emulation direction. Present 2–4 options with a recommendation; research and diagnosis may continue while
  waiting, applying the choice may not. A request to fix something is not agreement with a particular approach.
- **Native over emulation;** model hardware only where the runtime must. Prefer the game's own Dimps engine / SDK code
  over runtime HLE stubs; unbind inaccurate HLE stubs so the game's real SDK code executes whenever hardware MMIO is
  not directly involved. Find root causes; no workarounds that paper over symptoms. Survey existing approaches before inventing.
- **Performance changes must give identical output** (same frames, same sound, exact frame advantage & cancel timing in combat).
  Test at **1x**: every run passes `PS2X_GS_SCALE=1 PS2X_GS_SSAA=1` and you check the `[output] scale=1x` log line.
  Two runs of the same build may not match in battle today (host-time dependence, issue 022): then record the
  comparison as pending, never as passed. A speed spec may still close with that comparison pending when byte-level
  evidence exists (CTests, 0 verifier mismatches, IDENTICAL boot frames), as DW5 did (user, 2026-10-29).
- **Exact saved-frame comparison (DW5 2026-10-22; tool planned: 014):** Use `tools/debug/compare_frames.py` for renderer/shader,
  VU graphics math, framebuffer/presentation changes, optimizations/refactors in those paths, and visual-regression
  investigations. This is a targeted visual debugging/correctness check, not a requirement after every change.
  Compare a small set of corresponding before/after frames covering the affected scene, with the same input/frame,
  machine/GPU/driver, output kind and settings (Vulkan, scale=1, SSAA=1). For renderer changes, replay the same saved
  GS recording in both builds. For game/VU changes, capture matching output from each build so the changed code
  actually runs; replaying an old GS recording bypasses those changes. If matching evidence is unavailable,
  record the comparison as pending and explain why; unrelated frames cannot establish output equality.
  Output-preserving optimizations/refactors must be IDENTICAL. For intended graphics fixes, inspect the heatmap
  and explain the expected differences against the approved spec. Record the report, heatmap and capture details
  in the issue's work log; use `--context` to check declared correspondence. FPS/frame-time measurements assess
  performance; profilers locate costs and waits. This tool compares pixels, not FPS. Capture images separately from
  timing runs: saving frames can itself cause stutter. Do not require this comparison for documentation, packaging,
  CI, launcher-only, audio-only or other changes unrelated to rendered output. It does not automatically trigger a
  full gameplay suite or replace existing sound/gameplay checks. (`tools/diff_frames.py` stays the tolerant
  port-vs-PCSX2 comparison.)
- **Function-map and generated-code validation (DW5 2026-10-24; tool planned: 014):** Use `tools/debug/validate_function_map.py`
  when changing function maps, Ghidra exports, entry-point configuration, or recompiler logic affecting
  boundaries, branch exits or delay slots. Also use it during missing-target or omitted-instruction
  investigations. Check edited maps before regeneration; audit corresponding generated output afterwards.
  Record the selected passes, findings and coverage limits in the issue's work log. Unrelated changes,
  ordinary play and FPS profiling do not require it.
- **GPU renderer only** (Vulkan/paraLLEl-GS) for development and tests; no CPU-renderer work unless asked.
- **The port is not specific to one machine.** Keep fixes portable across supported hardware and the
  planned Linux/Windows targets. The test machine's CPU/GPU are test equipment, not product requirements. Label
  performance measurements with their test hardware; do not generalize one machine's timings or introduce
  machine-specific fixes. Correctness is defined by PS2 behaviour.
- **The user's desktop is off limits:** never send synthetic input to it, never screenshot it (use frame dumps),
  never unlock it, never edit `~/.config/PCSX2`. Visible windows only for a maintainer test session or when asked.
- **One game at a time;** no builds or test runs while the user has a game window open. Tests use the silent sink
  `dbziw_codex_tests`; live play uses the default output. Never change audio defaults or route other apps.
- **Downloads need explicit permission** (name, source, size).
- **Nothing derived from the game in Git:** no game files (`game/`, `discs/`), Ghidra DBs, generated or lifted C++
  (`recomp/output/`, `recomp/vu0/`, `recomp/vu1/`), frame dumps, captures, AFS/SFD audio/video streams, microcode `.bin`,
  screenshots. Scan before every commit/push. Hooks in `.githooks/` *(planned: 017)* (`git config core.hooksPath .githooks`);
  never bypass them (`--no-verify`).
- **Releases (DW5 011 v5, 2026-10-19):** a release contains only our code and freely licensed tools (the builder:
  setup wizard, recompiler, sources, compiler) — never the compiled game, disc files, an ISO, generated or lifted C++
  or any other game-derived file. The player's PC builds the game (PS2Recomp is GPL-3.0; a shipped game binary would
  break it). Shipping the compiled game only with written permission from PS2Recomp's authors. Follow the proven DW5
  architecture (`app/setup/`: `disc_setup`, `setup_pipeline`, `wizard_ui`, `wizard_console`): the setup wizard
  verifies the player's retail disc (`SLES_553.47`), prepares `%LOCALAPPDATA%\DBZIW` (Windows) or
  `~/.local/share/dbziw` (Linux; `DBZIW_DATA_DIR` overrides), and compiles locally. Saves (`mc0/`, `mc1/`) must
  survive every update. Releases are published only by the user.
- **Commit identity:** author/committer `TheSlackbringer <15327900+TheSlackbringer@users.noreply.github.com>`;
  end messages with `Co-Authored-By: Claude <model> <noreply@anthropic.com>`. Call our renderer fixes "runtime
  patches" (`vendor/`). `external/PS2Recomp/` is vendored: runtime changes are ordinary commits here.
- **Use the Ghidra MCP whenever you can** (`mcp__ghidra__*` tools: decompile, xrefs, strings, renames/comments)
  for reverse engineering; call `get_program_info` first. Headless only when the MCP is unreachable (GUI closed),
  for batch jobs, or for parallel agents — always on a copy (see Reference → Ghidra). If the needed program is not
  the active tab, ask the user to switch it.
- **Parallel agents** get self-contained briefs and do analysis/patch-writing in their own worktree or scratch dir
  (Ghidra on their own copy); one integrator builds and runs the game. Each worktree gets its own `app/build-<task>`.
- **Never delete build dirs containing `_deps`;** no `rm` with bare shell variables (use `"${VAR:?}"`).
- **Test runs never change your settings or saves:** scratch copies of the settings file (`PS2X_SETTINGS_FILE`,
  *planned: 028*) and of the memory cards; check afterwards that yours are unchanged (DW5, 2026-10-22 and 2026-10-27).
- **Runnable test scripts live in `notes/issues/tests/`,** never only in the session scratchpad, which gets wiped
  (DW5, 2026-10-27).
- Keep `AGENTS.md` current when rules or the way of working change: propose the change, and edit only with the
  user's permission for that edit (DW5's rule). Private rules go to `notes/AGENTS.md`, never here. Check usage
  before expensive batches; at ≤10 % left, checkpoint (`work.md`, index) before continuing.

## Essentials
- Build: `cmake --build app/build` (all), `--target dbziw` (game), `--target dbziw_setup` (setup wizard).
  Regenerate code: `tools/regen.sh` (needs the player's disc in `game/`). CTests: `cd app/build && ctest`.
- **Preflight before any timing or play run:** `PS2X_ENABLE_AGRESSIVE_LOGS:BOOL=OFF` in the build tree's
  `CMakeCache.txt`, 0 `PS2_FUNCTION_LOG_TRACKER` in `compile_commands.json`, and no `ps2_log.txt` written (DW5 038:
  a build with it ON writes GBs of log and runs in slow motion while `[hud]` still shows full FPS). Since 009,
  `app/CMakeLists.txt:26` forces it OFF for game builds, but the runtime's own default is still ON for standalone
  runtime/recompiler builds (`ps2xRuntime/CMakeLists.txt:14`; fix: issue 011), and `app/build-mingw` was configured
  on 2026-10-20, before that, so its cache still says ON until it is reconfigured. An old `game/ps2_log.txt`
  (2026-10-20) exists: check the file's time, not only that it exists.
- Stub candidates: `tools/audit_stubs.py game/SLES_553.47 recomp/dbz_iw.toml`.
- Run all fast checks plus checks relevant to the changed area (testing rule above). Available issue tiers:
  `run.py NNN|quick|standard|full`; recommend full after at least three specs are fixed and ask the user before running it.
- Visual diffs vs PCSX2: `tools/diff_frames.py <port_frame.png> <pcsx2_ref.png> [--threshold 0.02]`.
- Unsure how the original looks or behaves: compare with PCSX2 first (`tools/pcsx2_reference.py` *(planned: 017)*),
  then ask the user for what screenshots can't show (feel, combat input response, sound timing).
- Debug code: `DBZIW-DEBUG` (kept, env-gated; the vendored runtime still says `DW5XL-DEBUG` until 017) and
  `TEMP-DEBUG` (remove before committing).

## Reference

### Repository map
- `external/PS2Recomp/` — vendored upstream runtime/recompiler plus our runtime fixes (ordinary files, copied from
  `dw5-ps2` at the fork; `DW5XL_VENDORED.md` is DW5's provenance note, to be replaced in 017).
  - `ps2xRuntime/` — EE runtime: memory, DMA, VIF, VU0/VU1 (interpreter `src/lib/vu/ps2_vu1_core.cpp`, lifted-code
    support `include/runtime/ps2_vu1_lift.h`), GS front end + Vulkan paraLLEl-GS backend, kernel/syscalls, SDK HLE
    stubs (`src/lib/Kernel/Stubs/`, incl. the libpad HLE `Pad.cpp` + `src/lib/ps2_pad.cpp`), EE scheduler
    (`src/lib/Kernel/EeScheduler.cpp`, incl. the stall watchdog), async device queue (`src/lib/ps2_device_queue.cpp`),
    GIF arbiter + GS worker (`src/lib/gs/ps2_gif_arbiter.cpp`).
  - `ps2xIOP/` — IOP R3000 interpreter running the game's real IRX modules (`CRI_ADXI.IRX`, `LIBSD.IRX`, `PADMAN.IRX`,
    `SIO2MAN.IRX`, `MCMAN.IRX`, `MCSERV.IRX`, `MODMIDI.IRX`, `AMSOUND.IRX`, `MODHSYN.IRX`, `SDRDRV.IRX`, `MODSEIN.IRX`,
    `MODSESQ.IRX`; `IOPRP310.IMG` handling: issue 020), IOP kernel `src/emulator/core/iop_kernel.cpp`, SPU2 model
    `src/emulator/core/iop_spu2.cpp`.
  - `ps2xRecomp/` — the recompiler `ps2_recomp` (VU0 macro ops: `src/lib/vu_translator.cpp`).
- `recomp/` — recompiler configuration (`dbz_iw.toml`: entry point `0x100008`, stubs, `[mmio]` map, jump tables,
  `[patches]`), `ghidra_functions.csv` (symbol boundaries across `0x100000`–`0x49C600`), and `recomp/output/`
  (generated C++, gitignored; `tools/regen.sh`). VU translator `app/setup/vu1lift/` and `recomp/vu1/` *(planned: 012)*.
- `app/` — the game target (built from generated code + the runtime's `main.cpp`) and the native setup pipeline:
  - `setup/` — first-launch setup wizard (`disc_setup`, `setup_pipeline`, `wizard_ui`, `wizard_console`,
    `build_game`, `install_state`, `codegen`).
  - `cmake/DbziwPlatform.cmake` — platform options and Windows DLL staging.
  - tests in `app/tests/` (CTest; today only `setup_disc` — the DW5 runtime tests: 018).
  - There is no `app/src/` yet: no game overrides, no CRI/Sofdec glue (020), no pad bridge (DBZIW uses the libpad HLE; 021).
- `tools/` — `regen.sh`, `audit_stubs.py`, `diff_frames.py`, `fix_ghidra_csv.py`, `make_checkpoint.py`,
  `ghidra_scripts/`, `windows/` (llvm-mingw toolchain, wine runner). *(planned: `pcsx2_reference.py`, `gsreplay/` — 017;
  `debug/` — 014; `profile/`, `perf_ab.py`, `thread_load.py` — 010; `release/` — 007)*
- `vendor/parallel-gs-patches/` — renderer runtime patches applied to `app/deps/parallel-gs` (README step;
  not yet tracked in Git — 017).
- Local, gitignored: `game/` (extracted PAL disc: `SLES_553.47`, `SYSTEM.CNF`, `MODULES/`, `USR/`, `mc0/`, `mc1/`
  memory cards), `discs/`, `ghidra/` (project `dbziw`), `notes/` (own local git repo: issues, evidence, history).

### How our code reaches the game
Generated C++ registers every game function under its PS2 address; calls go through that table. Overrides (none
yet) register per disc serial (`PS2_REGISTER_GAME_OVERRIDE(...)`, `ps2xRuntime/include/game_overrides.h`), verify the
expected instruction, then `runtime.replaceFunction(addr, fn)`; `runtime.lookupFunction(addr)` returns the original so
a replacement can wrap it. No mid-function hooks. Never patch disc files or generated output by hand. Only replace
functions that never wait; a replacement may continue into another guest function by setting `ctx->pc` (DW5,
2026-10-27). The bound `sceGsSyncPath` stub breaks this rule today (issue 027).

### Running the game by hand (prefer the issue tests and helpers)
From `game`: `PS2X_GS_BACKEND=vulkan PS2X_HIDDEN=1 PS2X_GS_SCALE=1 PS2X_GS_SSAA=1 PULSE_SINK=dbziw_codex_tests
PS2X_HOST_INPUT="<tick>:<KEY>:<hold>,..." PS2X_DUMP_DIR=<dir> PS2X_DUMP_EVERY=<n> PS2X_EXIT_TICK=<n> timeout <s>
../app/build/dbziw ./SLES_553.47 > run.log 2>&1`. Ticks are VSync fields (50/s in PAL50 mode, 59.94/s in PAL60
mode), so the same tick is a different moment in each mode (issue 019).
Keys: ENTER (Start), TAB (Select), Z (Square / Punch), V (Triangle / Kick), X or Space (Cross / Guard / Evade),
C (Circle / Ki Blast), Q/E (L1/R1 / Transformation & Aura Burst), Left/Right Shift (L2/R2 / Capsule Activate),
Arrows or WASD (D-pad / Left Stick). Leave ~100 ticks between menu presses.
A host-input script disables real keyboard polling for the whole run: never set `PS2X_HOST_INPUT`/`PS2X_INPUT` for
manual play. Silent sink missing after a reboot: `pactl load-module module-null-sink sink_name=dbziw_codex_tests`.
Stop a game with `kill <pid>` (never `pkill -f dbziw` from a shell whose own command line contains that string).
Known route until `dbztest.py` exists (017): `120:CROSS:5,400:CROSS:5,800:LEFT:5,850:CROSS:5,950:CROSS:5,1100:CROSS:5,
1250:CROSS:5,1400:CROSS:5` passes language select, memory card check, the 50/60 Hz selector (the run stays in PAL50;
which option LEFT picks is not checked, 019) and the autosave warning; the opening movie starts at ~1790 (020 M1;
checkpoint `notes/issues/tests/out/020-m1-probe/state_1780.bin`). Timeline with 009's presses
(`320:X:10,800:ENTER:10,950:ENTER:10,...`, PAL50): language select ~100, Bandai Namco 500–600, Dimps ~800, CRIWARE
1000–1200; other presses move the screens to other ticks.
Presented-window timing without touching the desktop: instead of `PS2X_HIDDEN=1`, prefix the game with
`gamescope --backend headless -W 2560 -H 1440 -w 2560 -h 1440 -r 240 --` (example test display). Frame dumps are the
presented frames (`frame_<tick>.png`); contact sheet:
`magick montage frame_*.png -tile 4x4 -geometry 256x179+2+2 -label '%f' sheet.png`.

### Crash & stall diagnostics
- **Stall watchdog (`PS2X_STALL_WATCHDOG=<s>`, `EeScheduler.cpp`):** if the EE thread runs for more than `s` seconds
  (default 3) without yielding, it prints the active thread id, PC, RA and SP symbolized via `ghidra_functions.csv`, a
  heuristic guest call stack (return-address candidates found on the stack), VSync tick, EE cycle and video mode.
  It does not print SIF packets or VU1 state (a fuller crash/hang report: issue 023).
- **Guest crash handler (`PS2X_CRASH_HANDLER=1`, `ps2_runtime.cpp`):** in-process fault report before aborting.

### Debug switches (env, all optional)
Diagnostics/pacing: `PS2X_STALL_WATCHDOG=3` · `PS2X_CRASH_HANDLER=1` · `PS2X_STATS` (vsync/s per second) ·
`PS2X_HUD_LOG` ([hud] fps/vsync/frameMs/maxFrameMs every 0.5 s) · `PS2X_LATE_VBLANK` (fields >3 ms late) ·
`PS2X_VU1_ASYNC_TRACE` (device worker busy/drains per second; "explicit=N" render syncs = 2 x game FPS) ·
`PS2X_DEVICE_PROFILE=1` with the async trace (device time split) ·
`PS2X_VU1_CENSUS=<file>` (per-VU1-program runs/cycles/host ns) · `PS2X_VU1_LIFT=0` (interpret all VU1) ·
`PS2X_VU1_LIFT_VERIFY=1` (every native VU1 run repeated by the interpreter; slow) · `PS2X_VU1_RUN_CAPTURE=<prefix>` ·
`PS2X_VU1_ASYNC=0` (synchronous device) · `PS2X_GS_ASYNC=0` (GS inline, no worker thread) · `PS2X_HOST_FPS` ·
`PS2X_LOAD_STATE=<file>` / `PS2X_DUMP_STATE=<file>` / `PS2X_DUMP_STATE_TICK=<n>` (checkpoints; full fidelity: 016) ·
`PS2X_CTRL=<file>` (dev command file).
Graphics: `PS2X_GS_SCALE` / `PS2X_GS_SSAA` · `PS2X_NO_WEAVE` · `PS2X_GS_SYNCREADBACK` · `PS2X_GS_TRACE=<file>` +
`_FROM/_TO` · `PS2X_GS_RECORD=<file>` + `_FROM/_TO` · `PS2X_VRAM_DUMP=<file>` · `PS2X_XGKICK_CAPTURE=<prefix>` + `_FROM/_TO`.
Sound/IOP: `PS2X_SPU2_TRACE` · `PS2X_LIBSD_TRACE` · `PS2X_AUDIO_DUMP=<wav>` (s16 stereo 48 kHz) · `PS2X_AUDIO_TRACE` ·
`PS2X_SIFAUDIO_TRACE` · `PS2X_MPEGAUDIO_TRACE` · `PS2X_IOP_HEAP_TRACE` · `PS2X_IO_TRACE` · `PS2X_IOP_CLOCK_TRACE` ·
`PS2X_IOP_CATCHUP_TRACE` (catch-ups > 2 ms, `[iop-catchup]`). Heavy IOP tracing starves audio streams: judge audio untraced.
Other: `PS2X_MPG_TRACE` · `PS2X_VIF1_CHAIN_TRACE` · `PS2X_RAM_DUMP_TICKS=t1,t2` · `PS2X_RAM_WATCH=addr:len`.
*Planned (not in the code yet):* `PS2X_VU0_LIFT`/`_VERIFY`/`PS2X_VU0_CENSUS` (012, only if VU0 microprograms run) ·
`PS2X_STDIO_DIRECT=1` (bypass the log pump, 011) · `PS2X_PRESENT_STATS`, `PS2X_FIELD_SCANOUT=0`, `PS2X_WEAVE_TRACE`
(013) · `PS2X_STATE_RESAVE=<file>` (016) · `PS2X_PROFILE_MARKERS=<file>` in `-DDBZIW_DEVELOPER_DIAGNOSTICS=ON`
builds (010) · `PS2X_MUTE=1` (Windows test runs without a silent sink, 011) · `PS2X_SETTINGS_FILE` (028) ·
CRI/Sofdec/IPU traces (020) ·
`PS2X_PAD_TRACE` (021) · `PS2X_STATE_HASH=<file>` (022).
Profiling: `perf` and `gprofng` are installed on the Linux test machine; the non-pausing toolkit is issue 010. The
profiler is a debugging tool, never a test: a profiled run never decides a pass or fail or a timing acceptance, and
its own tests are not in CI or the `run.py` tiers (DW5, 2026-10-22). Who-woke-whom stacks on Linux need a temporary
kernel setting (`perf_event_paranoid`): a system setting only the user changes.

### Comparing with the original (PCSX2)
`tools/pcsx2_reference.py --out <scratch> --seconds <n> [--interval 0.5] [--keys "<sec>:<keysym>,..."]`
*(planned: 017, ported from DW5)* runs PCSX2 headless on a private Xvfb display with a throwaway copy of its config
and the software renderer, taking F8 screenshots into `<out>/snaps`. Capture the same moment from the port and
compare side by side. Use `tools/diff_frames.py` for automated numerical and visual difference heatmaps. Screenshots
are game-derived (scratch or `notes/evidence/` only). Never touch the user's `~/.config/PCSX2` or send input to the
real desktop.

### Rendering bugs: record and replay one frame
`PS2X_GS_RECORD=<file> PS2X_GS_RECORD_FROM=<tick> PS2X_GS_RECORD_TO=<tick>` records every renderer call (starts with
a VRAM snapshot). Replay tools *(planned: 017)*: `app/build/dbziw_gsrender <rec> <cpu|vulkan> <outdir> <ticks> [--fb ...]`
replays it; bisect with `--stop-at`. `dbziw_gsrec dump <rec> --from N --count M --fields` lists the draws. Vulkan
renderer fixes are runtime patches in `vendor/parallel-gs-patches/`.

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
- IOP module bases at run time (Ghidra addresses are module-relative), known so far: `CRI_ADXI.IRX` 0x16a00 (020 M1).

### Pitfalls
- **Metrowerks CodeWarrior 2.4.1.01 peculiarities:** Dimps compiled Infinite World with Metrowerks CodeWarrior rather than
  GCC. Pay close attention to jump table layouts (switch tables), epilogue register restores, and function alignment.
- **Function boundaries:** Ghidra can merge a function into the one before it right after a `jr $ra` (DBZIW
  `0x168810`, found at runtime) or leave pointer-only targets in gaps (DW5 026); a CSV row ending on a branch used to
  get a fabricated `NOP` delay slot (DW5 038). Validate maps before regenerating (issue 014).
- **PAL50 vs PAL60 modes:** Infinite World boots into a 50Hz/60Hz selector screen. The runtime takes the field rate
  from the game's `SetGsCrt` (mode 3 = PAL; only modes 2/3 are recognised); the scanout guesses PAL from picture height.
  VSync cadence, CD stream rate, movies and IOP timing must all follow the selected mode (issue 019).
- **CRI ADX audio & Sofdec SFD video:** Infinite World relies heavily on CRI middleware (`CRI_ADXI.IRX`, AFS archives,
  and SFD video streams). Data transfer from IOP to EE streaming buffers must not starve or hitch. Sofdec runs on top
  of Sony's libmpeg and libipu (native game code, unbound), which drive the IPU hardware; the runtime's IPU is
  register-level only until 020's IPU model (option A, approved 2026-10-27) lands; update this line then.
  `Stubs/IPU.cpp` and `Stubs/MPEG.cpp` contain other games' fixed addresses (`MPEG.cpp` writes DW5's): never bind them
  for DBZIW.
- **Combat frame cancellation and juggle physics:** Dimps fighter mechanics (cancels, Aura Bursts, fatigue meters) rely on
  exact frame windows. Timing hacks or sleeps will desync combat physics and break combos.
- **Keep `[patches]` in `dbz_iw.toml` empty:** `ps2_analyzer` falsely flags ordinary stores as self-modifying code.
- **Runtime SDK HLE stubs often differ from the real library** (DW5: libgraph clears, libvu0 matrix order, libvif
  tags): prefer unbinding them so the game's own code runs; HLE only what touches hardware. DBZIW binds 190 functions
  today (issue 024).
- **VIF1 DIRECT** carries only its own payload; GIF PATH2 state continues IMAGE transfers across DIRECTs.
- **VU0 macro ops** are generated by `vu_translator.cpp`, separately from the (better tested) VU1 interpreter: when
  culling/visibility looks wrong on the EE side, compare the op with the VU1 version (DW5 VCLIP bug; DW5 017 flags,
  020 `BC2F`/`BC2T`). DBZIW uses VU0 macro mode heavily.
- **IOP RPC server functions run outside any IOP thread;** blocking waits there need the host-wait pump (DW5 004).
- **IOP time follows the host clock** (`iop_emulator.cpp` catch-up), so two runs of the same build can diverge (issue 022).
- **The async device worker is off by default** since 009 (`PS2X_VU1_ASYNC=0`, also forced in `run.py`) after a freeze
  in the bound `sceGsSyncPath`; DW5's speed relies on it being on (issue 027).
- **Never measure stutter with frame dumps on** (PNG dumps cause 40 ms hitches and can hide timing races); the
  overlay/`[hud]` VSync figure reads 48–51 by aliasing even when pacing is perfect — use late fields and game FPS.
- **Lifted VU1 code must stay cycle-exact** with the interpreter (Q/flag/XGKICK timing is observable by the game).
