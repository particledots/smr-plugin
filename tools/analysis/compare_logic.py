#!/usr/bin/env python3
"""Milestone 5: compare bounces made in Logic with the plugin run headless in CI (tools/analysis, tests/plugin_tests.cpp).

For every file M5_<mac>_<rate>_<setting>.wav in BOUNCES (setting: defaults or Qmax) the plugin renderer is run on
M5_in_<rate>.wav in INPUTS with the same setting, and the two are compared: alignment (best lag of the left channel,
within +-4000 samples), gain (least-squares, in dB), and the difference after alignment, relative to the render's rms.
The render has the plugin's reported latency trimmed. Logic's bounce may differ by a latency or two in alignment; that is reported.

Usage: compare_logic.py --render BIN --inputs DIR --bounces DIR [--work DIR]
       compare_logic.py --render BIN --inputs DIR --selftest    (makes fake bounces from the render: shifted, quantised to 24 bit, one at -3 dB)
"""
import argparse
import os
import re
import struct
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smrcap as C   # noqa: E402

NAME = re.compile(r"^M5_(.+)_(\d+)_(defaults|Qmax)\.wav$")
SETTINGS = {"defaults": [], "Qmax": ["--set", "q=1"]}
MAXLAG = 4000


def write24_stereo(path, x, rate):
    q = np.clip(np.round(x * 8388608.0), -8388608, 8388607).astype(np.int64) & 0xFFFFFF
    b = np.empty(q.shape + (3,), dtype=np.uint8)
    b[..., 0] = q & 0xFF
    b[..., 1] = (q >> 8) & 0xFF
    b[..., 2] = (q >> 16) & 0xFF
    data = b.tobytes()
    ch = x.shape[1]
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, ch, rate, rate * 3 * ch, 3 * ch, 24)
    hdr += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(hdr + data)


def render(binary, inwav, out, setting):
    cmd = [binary, inwav, out, "--tail", "12"] + SETTINGS[setting]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout)
        print(r.stderr, file=sys.stderr)
        raise SystemExit("render failed: " + " ".join(cmd))
    return r.stdout


def best_lag(ref, x):
    """Lag L such that x[n + L] best matches ref[n], searched within +-MAXLAG, by FFT cross-correlation."""
    n = min(len(ref), len(x), 1 << 21)
    nfft = 1 << int(np.ceil(np.log2(2 * n)))
    a = np.fft.rfft(ref[:n], nfft)
    b = np.fft.rfft(x[:n], nfft)
    cc = np.fft.irfft(b * np.conj(a), nfft)
    lags = np.concatenate([np.arange(0, MAXLAG + 1), np.arange(-MAXLAG, 0)])
    vals = np.concatenate([cc[:MAXLAG + 1], cc[nfft - MAXLAG:]])
    return int(lags[int(np.argmax(np.abs(vals)))])


def align(ref, x, lag):
    if lag >= 0:
        xa, ra = x[lag:], ref
    else:
        xa, ra = x, ref[-lag:]
    m = min(len(xa), len(ra))
    return ra[:m], xa[:m]


def rel_db(d, ref):
    s = float(np.sum(ref ** 2))
    return 10 * np.log10(max(float(np.sum(d ** 2)), 1e-300) / s) if s > 0 else float("nan")


def compare_one(name, ref, x):
    """ref and x: (frames, channels) float. Returns report lines."""
    lines = []
    if x.shape[1] != 2:
        lines.append("%s: the bounce has %d channel(s), the render has 2" % (name, x.shape[1]))
        if x.shape[1] == 1:
            x = np.repeat(x, 2, axis=1)
        else:
            return lines
    lag = best_lag(ref[:, 0], x[:, 0])
    lag1 = best_lag(ref[:, 1], x[:, 1])
    out = []
    for c, nm in ((0, "left"), (1, "right")):
        r, b = align(ref[:, c].astype(np.float64), x[:, c].astype(np.float64), lag)
        g = float(np.dot(r, b) / max(float(np.dot(r, r)), 1e-300))
        d1 = b - r
        dg = b - g * r
        out.append("%s: gain %+.3f dB, difference %.1f dB re signal (%.1f dB after fitting the gain), largest %.3g"
                   % (nm, 20 * np.log10(abs(g)) if g else float("nan"), rel_db(d1, r), rel_db(dg, r), float(np.max(np.abs(d1))) if len(d1) else 0.0))
    # Swapped channels: would show if left and right were exchanged somewhere.
    r0, b1 = align(ref[:, 0].astype(np.float64), x[:, 1].astype(np.float64), lag)
    swap = rel_db(b1 - r0, r0)
    lines.append("%s: lengths render %d, bounce %d; best lag of the bounce against the render %+d samples (right channel %+d). %s. Left against the bounce's right channel: %.1f dB."
                 % (name, len(ref), len(x), lag, lag1, "; ".join(out), swap))
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--inputs", required=True)
    ap.add_argument("--bounces")
    ap.add_argument("--work", default=None)
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    work = a.work or os.path.join(os.environ.get("RUNNER_TEMP", "/tmp"), "m5_compare")
    os.makedirs(work, exist_ok=True)

    if a.selftest:
        bdir = os.path.join(work, "fake_bounces")
        os.makedirs(bdir, exist_ok=True)
        fakes = [("M5_FAKEmac_96000_defaults.wav", 96000, "defaults", 37, 1.0), ("M5_FAKEmac_48000_Qmax.wav", 48000, "Qmax", 0, 0.70710678)]
        for fn, rate, setting, shift, gain in fakes:
            tmp = os.path.join(work, "fake_render.wav")
            render(a.render, os.path.join(a.inputs, "M5_in_%d.wav" % rate), tmp, setting)
            y = C.read_wav(tmp).x.astype(np.float64)
            y = np.vstack([np.zeros((shift, 2)), y]) * gain
            write24_stereo(os.path.join(bdir, fn), y, rate)
        a.bounces = bdir

    if not a.bounces:
        print("need --bounces or --selftest")
        return 2
    files = sorted(f for f in os.listdir(a.bounces) if NAME.match(f))
    if not files:
        C.emit("error", "compare_logic", ["no files named M5_<mac>_<rate>_<defaults|Qmax>.wav in " + a.bounces])
        return 1
    report = []
    for f in files:
        m = NAME.match(f)
        mac, rate, setting = m.group(1), int(m.group(2)), m.group(3)
        inwav = os.path.join(a.inputs, "M5_in_%d.wav" % rate)
        if not os.path.exists(inwav):
            report.append("%s: no input file for %d Hz" % (f, rate))
            continue
        tmp = os.path.join(work, "render_%d_%s.wav" % (rate, setting))
        render(a.render, inwav, tmp, setting)
        w = C.read_wav(tmp)
        b = C.read_wav(os.path.join(a.bounces, f))
        if b.sr != rate:
            report.append("%s: the bounce is %d Hz, expected %d Hz" % (f, b.sr, rate))
            continue
        report += compare_one(f, w.x.astype(np.float64), b.x.astype(np.float64))
    C.emit("notice", "compare_logic" + (" (self-test on fake bounces)" if a.selftest else ""), report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
