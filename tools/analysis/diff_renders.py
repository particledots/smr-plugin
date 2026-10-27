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
        # The two renders may be trimmed by latencies that differ by a sample (smr_render rounds, the plugin reports the next whole
        # number). Report the difference at the best shift of -3..+3 samples, and at shift 0.
        def stats(shift):
            if shift >= 0:
                xa, ya = x[shift:], y[:len(y) - shift] if shift else y
            else:
                xa, ya = x[:len(x) + shift], y[-shift:]
            m = min(len(xa), len(ya))
            xa, ya = xa[:m], ya[:m]
            d = xa - ya
            sig = float(np.sum(xa ** 2))
            rel = 10 * np.log10(max(float(np.sum(d ** 2)), 1e-300) / sig) if sig > 0 else float("nan")
            return int(np.sum(xa == ya)), m, rel, float(np.max(np.abs(d))) if m else 0.0
        s0 = stats(0)
        best = min(range(-3, 4), key=lambda k: stats(k)[2])
        sb = stats(best)
        lines.append("%s: lengths %d and %d. Shift 0: %d of %d identical, rms difference %.1f dB re signal. Best shift %+d (of the second against the first, in samples): %d of %d identical, rms difference %.1f dB re signal, largest %.3g"
                     % (n, len(x), len(y), s0[0], s0[1], s0[2], best, sb[0], sb[1], sb[2], sb[3]))
    if not lines:
        C.emit("error", "diff_renders: " + a.label, ["no common file names"])
        return 1
    C.emit("notice", "diff_renders: " + a.label, lines)
    return 0


if __name__ == "__main__":
    sys.exit(main())
