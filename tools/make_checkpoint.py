#!/usr/bin/env python3
"""Checkpoint generator helper for Dragon Ball Z: Infinite World port.
Captures post-boot memory checkpoints to allow instant test execution in quick tier."""

import argparse
import os
import subprocess
import sys

def parse_args():
    parser = argparse.ArgumentParser(
        description="Capture deterministic state checkpoint for DBZ: Infinite World"
    )
    parser.add_argument(
        "--elf",
        default="game/SLES_553.47",
        help="Path to retail game ELF (default: game/SLES_553.47)"
    )
    parser.add_argument(
        "--tick",
        type=int,
        default=450,
        help="VSync tick at which to capture checkpoint (default: 450)"
    )
    parser.add_argument(
        "--out",
        default="game/boot.state",
        help="Output path for state file (default: game/boot.state)"
    )
    parser.add_argument(
        "--keys",
        default="",
        help="Optional scripted host inputs: tick:BTN:dur,... (e.g. '150:START:6,300:CROSS:6')"
    )
    return parser.parse_args()

def main():
    args = parse_args()
    repo_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    elf_path = os.path.abspath(os.path.join(repo_dir, args.elf))
    out_path = os.path.abspath(os.path.join(repo_dir, args.out))
    bin_path = os.path.abspath(os.path.join(repo_dir, "app/build/dbziw"))

    if not os.path.isfile(elf_path):
        print(f"ERROR: Retail game ELF not found at {elf_path}", file=sys.stderr)
        return 1

    if not os.path.isfile(bin_path):
        print(f"Notice: dbziw game executable not yet compiled at {bin_path}.", file=sys.stderr)
        print("Build the game target first with: cmake --build app/build --target dbziw", file=sys.stderr)
        return 2

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    env = os.environ.copy()
    env["PS2X_DUMP_STATE"] = out_path
    env["PS2X_DUMP_STATE_TICK"] = str(args.tick)
    env["PS2X_EXIT_TICK"] = str(args.tick)
    env["PS2X_HIDDEN"] = "1"
    env["PS2X_GS_SCALE"] = "1"
    env["PS2X_GS_SSAA"] = "1"
    env["PULSE_SINK"] = "dbziw_codex_tests"
    if args.keys:
        env["PS2X_INPUT"] = args.keys

    print(f"Generating checkpoint at tick {args.tick} into {out_path}...")
    proc = subprocess.run([bin_path, elf_path], cwd=os.path.dirname(elf_path), env=env)

    if proc.returncode != 0:
        print(f"ERROR: Game process exited with returncode {proc.returncode}", file=sys.stderr)
        return 1

    if not os.path.isfile(out_path) or os.path.getsize(out_path) == 0:
        print(f"ERROR: Checkpoint file was not created at {out_path}", file=sys.stderr)
        return 1

    size_mb = os.path.getsize(out_path) / (1024 * 1024)
    print(f"SUCCESS: Checkpoint captured successfully ({size_mb:.2f} MB) at {out_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
