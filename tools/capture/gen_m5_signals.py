#!/usr/bin/env python3
"""Milestone 5: the test inputs for the Logic recordings of the plugin (docs/testing-milestone-5.md).

Writes M5_in_96000.wav and M5_in_48000.wav into OUTDIR: mono, 24-bit PCM, 30 s each. Uniform white noise at amplitude 0.03
(about -35 dBFS rms) for the first 4 s, then silence, so the resonators ring out. Fixed seeds: the same files every time.
24-bit on purpose: Logic and the offline renderer read exactly the same sample values.

Usage: gen_m5_signals.py OUTDIR
"""
import os
import struct
import sys

import numpy as np

SECONDS = 30
NOISE_SECONDS = 4
AMPLITUDE = 0.03


def make(rate, seed):
    rng = np.random.default_rng(seed)
    x = np.zeros(rate * SECONDS)
    n = rate * NOISE_SECONDS
    x[:n] = AMPLITUDE * rng.uniform(-1.0, 1.0, n)
    return np.clip(np.round(x * 8388608.0), -8388608, 8388607).astype(np.int32)


def write24(path, q, rate):
    b = np.empty((len(q), 3), dtype=np.uint8)
    u = q.astype(np.int64) & 0xFFFFFF
    b[:, 0] = u & 0xFF
    b[:, 1] = (u >> 8) & 0xFF
    b[:, 2] = (u >> 16) & 0xFF
    data = b.tobytes()
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 3, 3, 24)
    hdr += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(hdr + data)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for rate, seed in ((96000, 960), (48000, 480)):
        p = os.path.join(out, "M5_in_%d.wav" % rate)
        write24(p, make(rate, seed), rate)
        print("wrote", p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
