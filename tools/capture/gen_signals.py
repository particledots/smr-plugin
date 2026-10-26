#!/usr/bin/env python3
"""Generate the Milestone 3 (Tier A) test signals for recording through a real SMR.

Writes two stereo 96 kHz 24-bit WAV files (left = signal, right = silence, so Logic sends it to output 1 without pan-law loss) and prints SHA-256 and level checks.
Deterministic (fixed seed). Needs numpy only.   Usage: gen_signals.py OUTDIR
Level design (derived from Focusrite's published figures, not measured): peak never exceeds 0.5 (-6 dBFS).
"""
import hashlib, os, struct, sys
import numpy as np

FS = 96000
PEAK = 0.5          # hard limit, -6 dBFS


def db(x):
    return 10.0 ** (x / 20.0)


def write_wav24(path, x):
    x = np.clip(x, -1.0, 1.0)
    q = np.round(x * 8388607.0).astype(np.int32)
    q = np.stack([q, np.zeros_like(q)], axis=1).reshape(-1)   # stereo: left = signal, right = silence
    b = np.empty((len(q), 3), dtype=np.uint8)
    b[:, 0] = q & 0xFF
    b[:, 1] = (q >> 8) & 0xFF
    b[:, 2] = (q >> 16) & 0xFF
    data = b.tobytes()
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 2, FS, FS * 6, 6, 24)
    hdr += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(hdr + data)
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def sil(sec):
    return np.zeros(int(round(sec * FS)))


def noise(rng, sec, rms_dbfs):
    n = rng.standard_normal(int(round(sec * FS))) * db(rms_dbfs)
    return np.clip(n, -PEAK, PEAK)


def sweep():
    # exponential sine sweep 20 Hz -> 20 kHz over 30 s, -12 dBFS, 50 ms raised-cosine ends
    T, f1, f2 = 30.0, 20.0, 20000.0
    t = np.arange(int(T * FS)) / FS
    L = T / np.log(f2 / f1)
    s = np.sin(2 * np.pi * f1 * L * (np.exp(t / L) - 1.0)) * db(-12)
    k = int(0.05 * FS)
    w = 0.5 - 0.5 * np.cos(np.pi * np.arange(k) / k)
    s[:k] *= w
    s[-k:] *= w[::-1]
    return np.concatenate([sil(3), s, sil(5)])


def main_sig(rng):
    parts = [sil(2)]
    parts.append(noise(rng, 20, -20))                 # A: noise, 20 s, -20 dBFS RMS
    parts.append(sil(3))
    for _ in range(5):                                # B: five single-sample impulses, 6 s apart
        p = sil(6)
        p[int(0.5 * FS)] = PEAK
        parts.append(p)
    for lvl in (-60, -54, -48, -42, -36, -30, -24, -20):   # C: level steps, 5 s noise + 1 s silence
        parts.append(noise(rng, 5, lvl))
        parts.append(sil(1))
    parts.append(sil(2))
    return np.concatenate(parts)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    rng = np.random.default_rng(20261025)
    for name, x in (("smr_sig_sweep.wav", sweep()), ("smr_sig_main.wav", main_sig(rng))):
        h = write_wav24(os.path.join(out, name), x)
        rms = 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)
        print(f"{name}: {len(x) / FS:.1f} s, peak {20 * np.log10(np.max(np.abs(x))):.2f} dBFS, "
              f"overall RMS {rms:.1f} dBFS, sha256 {h}")


if __name__ == "__main__":
    main()
