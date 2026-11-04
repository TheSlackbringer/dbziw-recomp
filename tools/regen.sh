#!/usr/bin/env bash
# Generate locally from your own retail disc; never hand-edit or commit generated C++.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)

recompiler="$root/external/PS2Recomp/build/ps2xRecomp/ps2_recomp"
config="$root/recomp/dbz_iw.toml"
executable="SLES_553.47"
expected_sha256="afe8c7b99f31e635750a17618ae8248d1a64fe3ad0669996990abdf33e1819f4"

if [[ ! -f "$root/game/$executable" ]]; then
    echo "Missing your retail disc file: game/$executable" >&2
    exit 1
fi

actual_sha256=$(sha256sum "$root/game/$executable" | awk '{print $1}')
if [[ "$actual_sha256" != "$expected_sha256" ]]; then
    echo "Unsupported $executable; SHA256 $actual_sha256 does not match expected retail PAL disc ($expected_sha256)" >&2
    exit 1
fi

if [[ ! -x "$recompiler" ]]; then
    echo "Recompiler binary missing or not executable: $recompiler" >&2
    echo "Build it with: cmake --build external/PS2Recomp/build --target ps2_recomp" >&2
    exit 1
fi

if [[ ! -f "$config" ]]; then
    echo "Recompiler configuration missing: $config" >&2
    exit 1
fi

echo "== Generating game C++ from game/$executable..."
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT

# The paths go into the recompiler's config file: on Windows (Git Bash) it is a native program that cannot open
# /c/... paths, so write them as C:/... there.
native_path() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi; }

awk -v root="$(native_path "$root")" -v output="$(native_path "$tmp")/output/" '
    /^input = / { print "input = \"" root "/game/SLES_553.47\""; next }
    /^ghidra_output = / { print "ghidra_output = \"" root "/recomp/ghidra_functions.csv\""; next }
    /^output = / { print "output = \"" output "\""; next }
    { print }
' "$config" > "$tmp/config.toml"

mkdir -p "$tmp/output"
if ! "$recompiler" "$tmp/config.toml" > "$root/recomp/recomp.log" 2>&1; then
    echo "Generation failed; see recomp/recomp.log" >&2
    exit 1
fi

mkdir -p "$root/recomp/output"
if command -v rsync >/dev/null 2>&1; then
    rsync -rc --delete --itemize-changes "$tmp/output/" "$root/recomp/output/" > "$tmp/changes.txt"
else
    # No rsync (Git Bash on Windows): the same sync in Python. Only changed files are copied, so unchanged ones keep
    # their times and the game build stays incremental; files the generation no longer writes are removed.
    python=""
    for candidate in python3 python; do
        if "$candidate" -c 'import sys' >/dev/null 2>&1; then python="$candidate"; break; fi
    done
    if [[ -z "$python" ]]; then
        echo "Neither rsync nor Python found to copy the generated files" >&2
        exit 1
    fi
    "$python" - "$(native_path "$tmp")/output" "$(native_path "$root")/recomp/output" > "$tmp/changes.txt" <<'PY'
import filecmp, shutil, sys
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
fresh = {p.relative_to(src) for p in src.rglob('*') if p.is_file()}
old = {p.relative_to(dst) for p in dst.rglob('*') if p.is_file()}
for rel in sorted(old - fresh):
    (dst / rel).unlink()
    print(f'*deleting {rel.as_posix()}')
for rel in sorted(fresh):
    target = dst / rel
    if not target.is_file() or not filecmp.cmp(src / rel, target, shallow=False):
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src / rel, target)
        print(f'>f {rel.as_posix()}')
PY
fi
changes_count=$(wc -l < "$tmp/changes.txt")
echo "Code generation successful: $changes_count files updated in recomp/output/"
head -20 "$tmp/changes.txt"
echo "Report summary:"
grep -E "(Functions discovered|Functions processed|Indirect fallback promotions|Unhandled instructions|Errors:|Warnings:)" "$root/recomp/recomp.log" || true

# Translate the VU1 microprograms from your own PAL executable into ignored local C++ (issue 012; DW5 025 Step 6; never
# commit it). Build the translator first: cmake --build app/build --target dbziw_vu1lift. Stops on input or
# translation errors rather than compiling stale translated code; the file is replaced only when its text changed.
lifter="${DBZIW_VU1LIFT:-$root/app/build/dbziw_vu1lift}"
[[ -x "$lifter" || ! -x "$lifter.exe" ]] || lifter="$lifter.exe"
if [[ ! -x "$lifter" ]]; then
    echo "VU1 translator missing: $lifter (cmake --build app/build --target dbziw_vu1lift)" >&2
    exit 1
fi
echo "== Translating VU1 microprograms from game/$executable..."
mkdir -p "$root/recomp/vu1"
# Writes recomp/vu1/dbziw_vu1_lifted_NN.cpp (only files whose text changed; stale parts removed).
"$lifter" "$(native_path "$root/game/$executable")" "$(native_path "$root/recomp/vu1")" > "$tmp/vu1lift.log"
grep -c '^Translating' "$tmp/vu1lift.log" | sed 's/$/ VU1 programs translated/'
tail -1 "$tmp/vu1lift.log"
