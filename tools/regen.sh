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

awk -v root="$root" -v output="$tmp/output/" '
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
rsync -rc --delete --itemize-changes "$tmp/output/" "$root/recomp/output/" > "$tmp/changes.txt"
changes_count=$(wc -l < "$tmp/changes.txt")
echo "Code generation successful: $changes_count files updated in recomp/output/"
head -20 "$tmp/changes.txt"
echo "Report summary:"
grep -E "(Functions discovered|Functions processed|Indirect fallback promotions|Unhandled instructions|Errors:|Warnings:)" "$root/recomp/recomp.log" || true
