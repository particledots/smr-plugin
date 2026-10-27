"""Shared helpers for the Milestone 3 hardware-capture analysis (WAV reader, file-name matcher, signal timeline, annotations).

Plain Python, needs numpy only. No plugin or firmware code.
"""
import os
import re
import struct
from collections import namedtuple

import numpy as np

FS = 96000
Wav = namedtuple("Wav", "x sr bits tag chunks")

# Signal timeline in seconds. Mirrors tools/capture/gen_signals.py (selftest.py checks this against the generator).
SWEEP = dict(start=3.0, dur=30.0, f1=20.0, f2=20000.0, total=38.0)
MAIN = dict(
    noiseA=(2.0, 22.0),
    impulses=[25.5 + 6.0 * k for k in range(5)],
    steps_db=[-60, -54, -48, -42, -36, -30, -24, -20],
    step_t0=55.0,
    step_period=6.0,
    step_noise=5.0,
    total=105.0,
)

# Pass table from docs/capture-sheet.md section E.
PASSES = {
    "P0": dict(kind="noise", q="min", sliders="all (nothing patched in)"),
    "P1": dict(kind="sweep", q="mid", sliders="1"),
    "P2": dict(kind="sweep", q="mid", sliders="2"),
    "P3a": dict(kind="main", q="min", sliders="all"),
    "P3b": dict(kind="main", q="mid", sliders="all"),
    "P3c": dict(kind="main", q="max", sliders="all"),
    "P4": dict(kind="main", q="max", sliders="all", rotate=10),
    "P5": dict(kind="loopback", optional=True),
}
JACK_NAME = {1: "odds", 2: "evens"}   # Scarlett input 1 = SMR OUT odds jack, input 2 = SMR OUT evens jack


def read_wav(path):
    """Read a PCM or float WAV, tolerating extra chunks (JUNK, bext, ...). Returns Wav with x as float32 (frames, channels)."""
    with open(path, "rb") as f:
        b = f.read()
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file")
    pos, fmt, data, chunks = 12, None, None, []
    while pos + 8 <= len(b):
        cid = b[pos:pos + 4]
        sz = struct.unpack("<I", b[pos + 4:pos + 8])[0]
        body = b[pos + 8:pos + 8 + sz]
        chunks.append((cid.decode("latin1"), sz))
        if cid == b"fmt ":
            tag, ch, sr, _br, _ba, bits = struct.unpack("<HHIIHH", body[:16])
            if tag == 0xFFFE and len(body) >= 26:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, ch, sr, bits)
        elif cid == b"data":
            data = body
        pos += 8 + sz + (sz & 1)
    if fmt is None or data is None:
        raise ValueError("missing fmt or data chunk")
    tag, ch, sr, bits = fmt
    if tag == 1 and bits == 24:
        a = np.frombuffer(data[:len(data) // 3 * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = a[:, 0] | (a[:, 1] << 8) | (a[:, 2] << 16)
        v = np.where(v & 0x800000, v - 0x1000000, v)
        x = (v / 8388608.0).astype(np.float32)
    elif tag == 1 and bits == 16:
        x = (np.frombuffer(data[:len(data) // 2 * 2], dtype="<i2") / 32768.0).astype(np.float32)
    elif tag == 1 and bits == 32:
        x = (np.frombuffer(data[:len(data) // 4 * 4], dtype="<i4") / 2147483648.0).astype(np.float32)
    elif tag == 3 and bits == 32:
        x = np.frombuffer(data[:len(data) // 4 * 4], dtype="<f4").astype(np.float32)
    else:
        raise ValueError("unsupported WAV format tag %d, %d bits" % (tag, bits))
    n = len(x) // ch
    return Wav(x[:n * ch].reshape(n, ch), sr, bits, tag, chunks)


def main_input():
    """The exact smr_sig_main.wav signal as floats, regenerated from tools/capture/gen_signals.py (deterministic, fixed seed)."""
    import importlib.util
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "capture", "gen_signals.py")
    spec = importlib.util.spec_from_file_location("gen_signals", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.main_sig(np.random.default_rng(20261025))


def classify(name):
    """File name -> (pass id, jack) or None. jack 1 = odds (Scarlett input 1), 2 = evens (input 2).

    Rule decided by Bryn on 2026-10-26: any name containing "odds" is input 1 (P0 odds file was typed "in2" by mistake).
    Otherwise "even" means input 2, otherwise the in1/in2 text decides. The pass comes from the leading P-code, not from
    any other text (the P2 files are named "s1" but hold slider 2).
    """
    base = os.path.basename(name)
    m = re.match(r"^P(\d)([abc]?)", base, re.I)
    if not m:
        return None
    pid = "P" + m.group(1) + m.group(2).lower()
    low = base.lower()
    if pid == "P5":
        jack = 1
    elif "odds" in low:
        jack = 1
    elif "even" in low:
        jack = 2
    else:
        j = re.search(r"in([12])", low)
        jack = int(j.group(1)) if j else None
    return pid, jack


def note_name(f):
    """Nearest 12-TET note (A4 = 440 Hz) and the offset in cents. Only a label: SMR scales are not all 12-TET."""
    midi = 69 + 12 * np.log2(f / 440.0)
    k = int(round(midi))
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    return "%s%d" % (names[k % 12], k // 12 - 1), (midi - k) * 100.0


def cents(f, ref):
    return 1200.0 * np.log2(f / ref)


def _esc(s):
    return s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def emit(level, title, lines):
    """One GitHub Actions annotation per call (a step shows at most 10 per level, so pack lines together)."""
    if os.environ.get("GITHUB_ACTIONS"):
        t = _esc(title).replace(":", "%3A").replace(",", "%2C")
        print("::%s title=%s::%s" % (level, t, _esc("\n".join(lines))), flush=True)
    else:
        print("[%s] %s" % (level.upper(), title))
        for ln in lines:
            print("    " + ln)
        print(flush=True)
