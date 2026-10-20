#!/usr/bin/env python3
"""
tools/diff_frames.py — Automated Visual Baseline Diff Tool

Computes quantitative visual mismatch metrics (mismatched pixel ratio, MSE, PSNR, dHash)
between native port frame dumps and reference software captures (e.g. PCSX2 baseline),
generating an amplified visual diff heatmap.

Part of Dragon Ball Z: Infinite World native port (Issue 002).
"""

import argparse
import os
import sys
import numpy as np
from PIL import Image

def compute_dhash(image: Image.Image, hash_size: int = 8) -> int:
    """Computes 64-bit difference hash (dHash) for structural image comparison."""
    resized = image.convert("L").resize((hash_size + 1, hash_size), Image.Resampling.LANCZOS)
    pixels = np.array(resized)
    diff = pixels[:, 1:] > pixels[:, :-1]
    hash_val = 0
    for bit in diff.flatten():
        hash_val = (hash_val << 1) | int(bit)
    return hash_val

def hamming_distance(h1: int, h2: int) -> int:
    """Computes the Hamming distance between two 64-bit hashes."""
    return bin(h1 ^ h2).count("1")

def diff_images(path_a: str, path_b: str, threshold: float = 0.02, max_diff: float = 1.0,
                out_path: str = None, side_by_side: bool = False, quiet: bool = False) -> tuple[bool, dict]:
    """
    Compares two images and calculates metrics.
    
    threshold: per-pixel normalized Euclidean distance in RGB (0.0 to 1.0). Default 0.02 (2%).
    max_diff: maximum allowed percentage of mismatched pixels before failing (default 1.0%).
    """
    if not os.path.exists(path_a):
        print(f"Error: Image A not found: {path_a}", file=sys.stderr)
        return False, {}
    if not os.path.exists(path_b):
        print(f"Error: Image B not found: {path_b}", file=sys.stderr)
        return False, {}

    try:
        img_a = Image.open(path_a).convert("RGB")
        img_b = Image.open(path_b).convert("RGB")
    except Exception as e:
        print(f"Error loading images: {e}", file=sys.stderr)
        return False, {}

    if img_a.size != img_b.size:
        print(f"Error: Resolution mismatch: Image A is {img_a.size[0]}x{img_a.size[1]}, "
              f"Image B is {img_b.size[0]}x{img_b.size[1]}", file=sys.stderr)
        return False, {}

    w, h = img_a.size
    total_pixels = w * h

    arr_a = np.array(img_a, dtype=np.float32)
    arr_b = np.array(img_b, dtype=np.float32)

    # 1. Perceptual dHash
    hash_a = compute_dhash(img_a)
    hash_b = compute_dhash(img_b)
    dhash_dist = hamming_distance(hash_a, hash_b)

    # 2. Normalized Euclidean distance per pixel (0.0 to 1.0)
    diff_rgb = np.abs(arr_a - arr_b)
    euclidean = np.linalg.norm(diff_rgb, axis=-1) / (255.0 * np.sqrt(3.0))

    mismatched_mask = euclidean > threshold
    mismatched_count = int(np.count_nonzero(mismatched_mask))
    mismatch_pct = (mismatched_count / total_pixels) * 100.0

    # 3. MSE and PSNR
    mse = float(np.mean((arr_a - arr_b) ** 2))
    if mse < 1e-9:
        psnr = float("inf")
    else:
        psnr = float(20.0 * np.log10(255.0 / np.sqrt(mse)))

    passed = mismatch_pct <= max_diff

    # 4. Heatmap generation
    if out_path:
        # Faint grayscale base image (35% intensity)
        gray = (0.299 * arr_a[:, :, 0] + 0.587 * arr_a[:, :, 1] + 0.114 * arr_a[:, :, 2]) * 0.35
        heatmap = np.stack([gray, gray, gray], axis=-1)

        # Highlight mismatched pixels in bright magenta
        heatmap[mismatched_mask] = [255.0, 20.0, 147.0]
        heatmap_img = Image.fromarray(np.clip(heatmap, 0, 255).astype(np.uint8))

        if side_by_side:
            # Create 3-panel side-by-side: Image A | Image B | Heatmap
            combined = Image.new("RGB", (w * 3, h))
            combined.paste(img_a, (0, 0))
            combined.paste(img_b, (w, 0))
            combined.paste(heatmap_img, (w * 2, 0))
            combined.save(out_path)
        else:
            heatmap_img.save(out_path)

    metrics = {
        "width": w,
        "height": h,
        "total_pixels": total_pixels,
        "mismatched_pixels": mismatched_count,
        "mismatch_pct": mismatch_pct,
        "mse": mse,
        "psnr": psnr,
        "dhash_dist": dhash_dist,
        "passed": passed
    }

    if not quiet:
        print("=" * 80)
        print("FRAME DIFF REPORT")
        print("=" * 80)
        print(f"Image A:            {path_a} ({w}x{h})")
        print(f"Image B:            {path_b} ({w}x{h})")
        print(f"Threshold:          {threshold:.4f} ({threshold * 100:.2f}%)")
        print(f"Max Allowed Diff:   {max_diff:.2f}%")
        print("-" * 80)
        print("RESULTS:")
        print(f"  Mismatched Pixels:   {mismatched_count:,} / {total_pixels:,} ({mismatch_pct:.2f}%)")
        print(f"  MSE:                 {mse:.4f}")
        psnr_str = "inf dB" if np.isinf(psnr) else f"{psnr:.2f} dB"
        print(f"  PSNR:                {psnr_str}")
        print(f"  dHash Distance:      {dhash_dist} / 64")
        status_str = "PASS" if passed else f"FAIL (Exceeds max allowed diff of {max_diff:.2f}%)"
        print(f"  Status:              {status_str}")
        if out_path:
            print("-" * 80)
            print(f"Heatmap written to:  {out_path}")
        print("=" * 80)

    return passed, metrics

def main():
    parser = argparse.ArgumentParser(description="Automated visual baseline diff tool for frame dumps.")
    parser.add_argument("image_a", help="Path to native port frame dump (PNG)")
    parser.add_argument("image_b", help="Path to reference software frame snapshot (PNG)")
    parser.add_argument("--threshold", type=float, default=0.02,
                        help="Per-pixel normalized color distance threshold (default: 0.02)")
    parser.add_argument("--max-diff", type=float, default=1.0,
                        help="Maximum percentage of mismatched pixels before failing (default: 1.0%%)")
    parser.add_argument("--out", type=str, default=None,
                        help="Path to save diff heatmap PNG (default: None)")
    parser.add_argument("--side-by-side", action="store_true",
                        help="Save a 3-panel side-by-side image (Image A | Image B | Heatmap)")
    parser.add_argument("--quiet", action="store_true",
                        help="Suppress terminal report output")

    args = parser.parse_args()

    passed, _ = diff_images(
        path_a=args.image_a,
        path_b=args.image_b,
        threshold=args.threshold,
        max_diff=args.max_diff,
        out_path=args.out,
        side_by_side=args.side_by_side,
        quiet=args.quiet
    )

    sys.exit(0 if passed else 1)

if __name__ == "__main__":
    main()
