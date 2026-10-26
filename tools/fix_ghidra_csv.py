#!/usr/bin/env python3
"""Clean Ghidra's ExportPS2Functions CSV for ps2_recomp.

- drop rows at/after the end of code (data labels Ghidra marks executable)
- preserve all stubs declared in TOML (splitting if a stub starts inside a function body)
- prune overlapping child labels (entry_*, caseD_*, default) that lie inside parent functions
  so loops and control flow remain unified within single native C++ functions
- clamp functions whose End is far past Start+Size
usage: fix_ghidra_csv.py in.csv out.csv CODE_END_HEX [--toml config.toml]
"""
import os, re, sys

if len(sys.argv) < 4:
    print("Usage: fix_ghidra_csv.py in.csv out.csv CODE_END_HEX [--toml config.toml]", file=sys.stderr)
    sys.exit(1)

src = sys.argv[1]
dst = sys.argv[2]
code_end = int(sys.argv[3], 16)

toml_path = None
if "--toml" in sys.argv:
    idx = sys.argv.index("--toml")
    if idx + 1 < len(sys.argv):
        toml_path = sys.argv[idx + 1]
elif os.path.exists("recomp/dbz_iw.toml"):
    toml_path = "recomp/dbz_iw.toml"

stub_addrs = set()
if toml_path and os.path.exists(toml_path):
    with open(toml_path) as tf:
        stubs = re.findall(r"\"([^\"]+@0x([0-9a-fA-F]+))\"", tf.read())
        stub_addrs = {int(addr, 16) for _, addr in stubs}

raw_funcs = []
clamped = 0
with open(src) as f:
    for i, line in enumerate(f):
        if i == 0 or not line.strip():
            continue
        parts = line.rstrip('\n').split(',')
        name, start, end, size = parts[0], parts[1], parts[2], parts[3]
        s, e, n = int(start, 16), int(end, 16), int(size)
        if s >= code_end:
            continue
        if e - s > n + 0x400:
            e = s + n
            clamped += 1
        raw_funcs.append((name, s, e, n))

# Split any function if a declared stub address falls strictly inside its body
splits = []
for f in raw_funcs:
    name, s, e, sz = f
    stubs_inside = sorted([sa for sa in stub_addrs if s < sa < e])
    if stubs_inside:
        cur_s = s
        for sa in stubs_inside:
            splits.append((f"{name}_pre", cur_s, sa, sa - cur_s))
            cur_s = sa
        splits.append((f"{name}_stub", cur_s, e, e - cur_s))
    else:
        splits.append(f)

# Sort by start address asc, then size desc
splits.sort(key=lambda x: (x[1], -(x[2] - x[1])))

# Greedy non-overlapping interval selection
cleaned = []
cur_end = 0
dropped_labels = 0
for f in splits:
    name, s, e, sz = f
    if s >= cur_end:
        cleaned.append(f)
        cur_end = e
    elif e > cur_end and (e - cur_end) <= 16:
        # Extend parent by delay slot if needed
        last = cleaned[-1]
        cleaned[-1] = (last[0], last[1], e, e - last[1])
        cur_end = e
    else:
        dropped_labels += 1

out = ["Name,Start,End,Size\n"]
for f in cleaned:
    out.append(f"{f[0]},0x{f[1]:08X},0x{f[2]:08X},{f[3]}\n")

with open(dst, 'w') as f:
    f.writelines(out)

print(f"Cleaned {len(raw_funcs)} raw functions -> {len(cleaned)} unified functions ({dropped_labels} internal labels pruned, {clamped} clamped).", file=sys.stderr)
