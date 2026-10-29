# Vendored PS2Recomp (modified)

This directory is a modified copy of [PS2Recomp](https://github.com/ran-j/PS2Recomp) (GPL-3.0, see `LICENSE`), vendored into the DW5XL port on 2026-10-13. It is not the unmodified upstream release.

- Upstream base: `ran-j/PS2Recomp` commit `c5a9d02573410a2085a4b4b831b0b68ba3515440` (2026-09-30).
- Applied on top: the PS2Recomp patch series kept in `../../vendor/runtime-patches/` (provenance in `../../THIRD_PARTY.md`), upstream pull requests #271 and #274, and the DW5XL runtime changes (VIF1 DIRECT/GIF PATH2 handling, libmpeg end-of-stream, PSS audio sub-stream filtering, memory card directory count, IOP SET_SREG forwarding, `sceCdBreak`, `GetSystemTimeLow`, PAL/NTSC cadence selection, paraLLEl-GS Vulkan compute renderer with 3-slot asynchronous readback ring buffer, and others).
- Changes after vendoring are recorded in this repository's Git history.
- Upstream sync for DBZIW, 2026-10-29: level with `ran-j/PS2Recomp` main at `e5636981a1c9de368f6b71e819c3dc188dfedbf4`
  (2026-10-23). Of the four upstream commits after the base, #271 and #274 were already applied (above) and #272
  (PS2 float semantics for EE FPU results and DIV.S/RSQRT.S, `eb8688b`) was already present in full (its translator,
  macros and tests, identical tests). #384 (GIF FLG=3 treated like IMAGE wherever a GIFtag is parsed, `e563698`) was
  applied, except its `gs_frontend.cpp` hunk: `processGIFPacket` here already maps FLG=3 to IMAGE when it reads the tag.

Build directories (`build/`) are local and ignored.

---

## Strategy for Getting Upstream Updates

We track upstream `ran-j/PS2Recomp` while keeping our vendored directory in-tree (matching Budokai Tenkaichi 3's in-tree monorepo model, rather than submodule forks like SOCOM or God of War).

### 1. Configure the Upstream Remote
```bash
git remote add upstream-ps2recomp https://github.com/ran-j/PS2Recomp.git
git fetch upstream-ps2recomp
```

### 2. Method A: Cherry-Picking Specific Fixes (Recommended for targeted updates)
When upstream fixes a specific bug or merges a pull request:
```bash
# Using subtree strategy to apply directly to external/PS2Recomp
git cherry-pick -Xsubtree=external/PS2Recomp <commit-hash>
```
Alternatively, via patch diff:
```bash
git format-patch -1 <commit-hash> --stdout | git apply --directory=external/PS2Recomp -
```

### 3. Method B: Git Subtree Sync (For major upstream version bumps)
To merge a full upstream release into `external/PS2Recomp`:
```bash
# Preview what changed between our base and upstream main:
git log c5a9d02..upstream-ps2recomp/main

# Pull and merge upstream changes into external/PS2Recomp:
git subtree pull --prefix=external/PS2Recomp upstream-ps2recomp main --squash
```

### 4. Method C: Upstream-Tracking Branch
A local branch `vendor/upstream-ps2recomp` can track `upstream-ps2recomp/main` directly. Whenever updating, compare `git diff vendor/upstream-ps2recomp main:external/PS2Recomp` to easily audit our custom game modifications against pristine upstream.

