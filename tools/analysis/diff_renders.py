#!/usr/bin/env python3
"""Milestone 5: compare two directories of port renders (same file names), for example the core through smr_render and the
plugin through smr_plugin_tests. Prints one annotation: per file, samples identical, rms difference relative to the
signal, largest difference. Says nothing about the hardware.

Usage: diff_renders.py DIR_A DIR_B [--label TEXT]
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smrcap as C   # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir_a")
    ap.add_argument("dir_b")
    ap.add_argument("--label", default="renders")
    a = ap.parse_args()
    names = sorted(f for f in os.listdir(a.dir_a) if f.endswith(".wav") and os.path.exists(os.path.join(a.dir_b, f)))
    lines = []
    for n in names:
        x = C.read_wav(os.path.join(a.dir_a, n)).x.astype(np.float64).ravel()
        y = C.read_wav(os.path.join(a.dir_b, n)).x.astype(np.float64).ravel()
        m = min(len(x), len(y))
        d = x[:m] - y[:m]
        same = int(np.sum(x[:m] == y[:m]))
        sig = float(np.sum(x[:m] ** 2))
        rel = 10 * np.log10(max(float(np.sum(d ** 2)), 1e-300) / sig) if sig > 0 else float("nan")
        lines.append("%s: lengths %d and %d, %d of %d samples identical, rms difference %.1f dB re signal, largest %.3g"
                     % (n, len(x), len(y), same, m, rel, float(np.max(np.abs(d))) if m else 0.0))
    if not lines:
        C.emit("error", "diff_renders: " + a.label, ["no common file names"])
        return 1
    C.emit("notice", "diff_renders: " + a.label, lines)
    return 0


if __name__ == "__main__":
    sys.exit(main())
