#!/usr/bin/env python3
"""Fake SMR hardware capture with KNOWN answers, for testing the analysis tool before any real data is trusted.

Not a model of the SMR. Each "channel" is a plain two-pole resonator with a chosen centre frequency and ring time.
Uses the real test signals from tools/capture/gen_signals.py, the real file names (typos included) and a Logic-like
WAV layout (extra chunks). Usage: fake_capture.py OUTDIR
"""
import importlib.util
import os
import struct
import sys

import numpy as np
from scipy.signal import lfilter

FS = 96000
HERE = os.path.dirname(os.path.abspath(__file__))

# Truth. (frequency Hz, linear amplitude). Channel 1 on the ODDS jack, channel 2 on the EVENS jack (as measured on the real unit).
ODDS = [(223.0, 0.08), (301.7, 0.06), (1210.4, 0.05)]
EVENS = [(415.0, 0.08), (560.2, 0.06), (2005.9, 0.05)]
ODDS_ROT = [(247.3, 0.08), (341.2, 0.06), (1370.8, 0.05)]
EVENS_ROT = [(469.9, 0.08), (633.4, 0.06), (2270.3, 0.05)]
T60 = {"min": 0.3, "mid": 0.7, "max": 1.5}
RING_A = 0.01   # amplitude of the ring left by a unit impulse, for the amp-0.08 mode
FLOOR_DBFS = -85.0
LATENCY = 480   # samples (5 ms)
SWEEP_GAIN = 0.05   # keeps the fake sweep outputs well below full scale

# Real file names from capture-1, typos included. (pass, jack) -> name
NAMES = {
    ("P0", 1): "P0-Noise_Qmin_in2_oddsOUTt.wav", ("P0", 2): "P0-Noise_Qmin_in2_evensOUT.wav",
    ("P1", 1): "P1_sweep_s1_Qmid_in1_oddsOUT.wav", ("P1", 2): "P1_sweep_s1_Qmid_in2_evenOUT.wav",
    ("P2", 1): "P2_sweep_s1_Qmid_in1_oddsOUT.wav", ("P2", 2): "P2_sweep_s1_Qmid_in2_evenOUT.wav",
    ("P3a", 1): "P3a_main_Qmin_in1_oddsOUT.wav", ("P3a", 2): "P3a_main_Qmin_in2_evenOUT..wav",
    ("P3b", 1): "P3b_main_Qmid_in1_oddsOUT.wav", ("P3b", 2): "P3b_main_Qmid_in2_evenOUT..wav",
    ("P3c", 1): "P3c_main_Qmax_in1_oddsOUT.wav", ("P3c", 2): "P3c_main_Qmax_in2_evenOUT.wav",
    ("P4", 1): "P4_main_rot10_Qmax_in1_oddsOUT.wav", ("P4", 2): "P4_main_rot10_Qmax_in2_evenOUT.wav",
}


def load_gen():
    spec = importlib.util.spec_from_file_location("gen_signals", os.path.join(HERE, "..", "capture", "gen_signals.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def write_wav24(path, x):
    """Mono 24-bit WAV with Logic-like extra chunks (JUNK first, odd-sized chunk and bext after the audio)."""
    q = np.round(np.clip(x, -1.0, 1.0 - 1e-7) * 8388607.0).astype(np.int32)
    b = np.empty((len(q), 3), dtype=np.uint8)
    b[:, 0] = q & 0xFF
    b[:, 1] = (q >> 8) & 0xFF
    b[:, 2] = (q >> 16) & 0xFF
    data = b.tobytes()

    def chunk(cid, body):
        return cid + struct.pack("<I", len(body)) + body + (b"\0" if len(body) & 1 else b"")

    body = b"WAVE" + chunk(b"JUNK", b"\0" * 64) + chunk(b"fmt ", struct.pack("<HHIIHH", 1, 1, FS, FS * 3, 3, 24)) + chunk(b"data", data)
    body += chunk(b"LGWV", b"\x01" * 4321) + chunk(b"bext", b"\x02" * 603)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def write_wav_pcm16(path, x, sr):
    q = np.round(np.clip(x, -1, 1 - 1e-4) * 32767).astype("<i2")
    d = q.tobytes()
    body = b"WAVE" + b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, sr, sr * 2, 2, 16) + b"data" + struct.pack("<I", len(d)) + d
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def resonator(x, f, t60, amp):
    th = 2 * np.pi * f / FS
    r = 10 ** (-3.0 / (t60 * FS))
    a = [1.0, -2 * r * np.cos(th), r * r]
    b = [1.0, 0.0, -1.0]
    # The two-pole resonator with numerator [1, 0, -1] leaves a ring of amplitude about 2 for a unit impulse, whatever its Q.
    return lfilter(np.array(b) * (RING_A * amp / 0.08 / 2.0), a, x)


def through(x, modes, q, rng, n_modes=3):
    y = np.zeros(len(x))
    for f, amp in modes[:n_modes]:
        y += resonator(x, f, T60[q], amp)
    y = np.concatenate([np.zeros(LATENCY), y])[:len(x)]
    return y + rng.standard_normal(len(x)) * 10 ** (FLOOR_DBFS / 20) + 1.53e-5


def build(outdir, seed=1):
    os.makedirs(outdir, exist_ok=True)
    g = load_gen()
    rng = np.random.default_rng(seed)
    sweep = g.sweep()
    main = g.main_sig(np.random.default_rng(20261025))
    n_sweep, n_main = int(36.9 * FS), int(104.7 * FS)
    truth = {}

    def put(key, y):
        write_wav24(os.path.join(outdir, NAMES[key]), y)

    # P0: nothing patched, a noise source of our own through the three resonators of each jack, Q min
    noise = rng.standard_normal(int(43.4 * FS)) * 10 ** (-30.0 / 20)
    put(("P0", 1), through(noise, ODDS, "min", rng))
    put(("P0", 2), through(rng.standard_normal(len(noise)) * 10 ** (-30.0 / 20), EVENS, "min", rng))
    # P1, P2: sweep, one slider up
    put(("P1", 1), through(sweep * SWEEP_GAIN, ODDS, "mid", rng, 1)[:n_sweep])
    put(("P1", 2), through(np.zeros_like(sweep), [], "mid", rng)[:n_sweep])
    put(("P2", 1), through(np.zeros_like(sweep), [], "mid", rng)[:n_sweep])
    put(("P2", 2), through(sweep * SWEEP_GAIN, EVENS, "mid", rng, 1)[:n_sweep])
    # P3, P4: main signal, all six up
    for pid, q, o, e in (("P3a", "min", ODDS, EVENS), ("P3b", "mid", ODDS, EVENS), ("P3c", "max", ODDS, EVENS), ("P4", "max", ODDS_ROT, EVENS_ROT)):
        put((pid, 1), through(main, o, q, rng)[:n_main])
        put((pid, 2), through(main, e, q, rng)[:n_main])
    truth["odds"], truth["evens"] = ODDS, EVENS
    truth["odds_rot"], truth["evens_rot"] = ODDS_ROT, EVENS_ROT
    truth["t60"] = T60
    return truth


if __name__ == "__main__":
    build(sys.argv[1])
