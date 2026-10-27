#!/usr/bin/env python3
"""Milestone 3: render the ported core through the same test signal as capture-1 and write the result in the same
file layout as the hardware recordings, so tools/analysis/analyse_capture.py measures both the same way.

Usage: port_passes.py --render PATH/smr_render --outdir DIR [--variant NAME] [--gain-db G]

What it does for each of the four main passes (P3a, P3b, P3c, P4):
  1. Regenerates the exact test signal (smrcap.main_input(), fixed seed) as a mono 96 kHz float WAV. Mono, because on the
     hardware the evens IN jack is normalled to the odds IN jack, so both input slots carry the signal.
  2. Runs smr_render with the panel as the capture sheet (docs/capture-sheet.md section C) and the per-channel bank,
     scale and note identified from capture-1 (docs/open-questions.md M3-f), at the Q setting of the pass.
     P4 sends 10 ROTATE clockwise steps before the noise starts.
  3. Splits the stereo output into two mono files named like the hardware files:
       right output (filtered_bufferR, channels 1/3/5)  -> ..._in1_oddsOUT.wav
       left output  (filtered_buffer,  channels 2/4/6)  -> ..._in2_evensOUT.wav
     This follows the hardware measurement (M3-a: slider 1 left by the ODDS jack), not the manual. Which host output
     the plugin finally uses is Bryn's later decision (M1-m).

Nothing here is a statement about the SMR; it is the port's side of a comparison. The level scale of the port is a
placeholder (M1-n), so the input is attenuated by --gain-db (default -30) to keep the output from saturating; the
comparison removes one common gain offset (see compare_port.py).
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

# Per-channel state identified from capture-1 (docs/open-questions.md M3-f): channel -> (bank, scale, note), notes at P3.
# Odds-jack channels 1, 3, 5: bank 6 (Western), notes 6, 8, 10; the scale cannot be told apart (the table values at these
# notes are the same in every scale), 2 is used. Evens-jack channels: 2 = bank 6 scale 10 note 7; 4 and 6 = scale 2,
# notes 9 and 11, bank 6 or bank 2 (the two differ by 0.17 cent, below what the measurement can separate).
BASE_CHANS = {1: (6, 2, 6), 2: (6, 10, 7), 3: (6, 2, 8), 4: (6, 2, 9), 5: (6, 2, 10), 6: (6, 2, 11)}
BANK2_CHANS = dict(BASE_CHANS)
BANK2_CHANS[4] = (2, 2, 9)
BANK2_CHANS[6] = (2, 2, 11)

# Freq jack offset on channels 1/3/5. The firmware adds trackoffset and BASE_TRACKOFFSET (-11) to the ADC count and
# the result passes a 40-point average and a bracket of +-2 counts (params.c, init_freq_lpf), so 15 gives a settled
# table index of 2 (4 - 2), which is +5.80 cent (exp_1voct.h). A hypothesis chosen to reproduce the +5.5 cent measured
# on the odds jack; the real cause is not known (open-questions.md M3-g).
ODDS_TRACKOFFSET = 15

VARIANTS = {
    # name: (channel table, trackoffset for odds or None, text)
    "base": (BASE_CHANS, None, "identified state, no frequency offset"),
    "offset": (BASE_CHANS, ODDS_TRACKOFFSET, "identified state, odds Freq-jack offset (+5.80 cent) as a stand-in for the measured +5.5 cent"),
    "bank2": (BANK2_CHANS, ODDS_TRACKOFFSET, "as offset, but channels 4 and 6 in bank 2 (Western equal)"),
}

PASSES = [  # id, file stem, Q pot count, rotate steps
    ("P3a", "P3a_main_Qmin", 0, 0),
    ("P3b", "P3b_main_Qmid", 2048, 0),
    ("P3c", "P3c_main_Qmax", 4095, 0),
    ("P4", "P4_main_rot10_Qmax", 4095, 10),
]


def write_wav_float_mono(path, x, sr=C.FS):
    x = np.asarray(x, dtype="<f4")
    data = x.tobytes()
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 3, 1, sr, sr * 4, 4, 32)
    hdr += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(hdr + data)


def render_pass(render, inwav, outdir, pid, stem, qpot, rot, chans, odds_off, gain_db, tmp, core_rate=None):
    stereo = os.path.join(tmp, stem + "_stereo.wav")
    cmd = [render, inwav, stereo, "--pot", "0=%d" % qpot, "--gain-db", "%g" % gain_db, "--tail", "10"]
    for ch, (b, s, n) in sorted(chans.items()):
        cmd += ["--chan", "%d=%d,%d,%d" % (ch, b, s, n)]
    if odds_off is not None:
        cmd += ["--trackoffset", "0=%d" % odds_off]
    if core_rate:   # "NUM/DEN": scheduler rate, file processed 1:1 (open question M2-b); see smr_render --help
        cmd += ["--core-rate", core_rate]
    if rot:
        script = os.path.join(tmp, stem + "_script.txt")
        with open(script, "w") as f:
            for k in range(rot):   # 0.05 s apart: the firmware turns each ROTATE flag into one step per main-loop pass
                f.write("%.2f rotup\n" % (0.30 + 0.05 * k))
        cmd += ["--script", script]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout)
        print(r.stderr, file=sys.stderr)
        raise SystemExit("smr_render failed for %s (exit %d)" % (pid, r.returncode))
    w = C.read_wav(stereo)
    if rot:
        os.remove(script)
    x = w.x
    if w.sr != C.FS or x.shape[1] != 2:
        raise SystemExit("unexpected renderer output: %d Hz, %d channels" % (w.sr, x.shape[1]))
    evens, odds = x[:, 0], x[:, 1]
    write_wav_float_mono(os.path.join(outdir, stem + "_in1_oddsOUT.wav"), odds)
    write_wav_float_mono(os.path.join(outdir, stem + "_in2_evensOUT.wav"), evens)
    os.remove(stereo)
    return r.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--outdir", required=True)
    ap.add_argument("--variant", default="base", choices=sorted(VARIANTS))
    ap.add_argument("--gain-db", type=float, default=-30.0)
    ap.add_argument("--core-rate", default=None, help="NUM/DEN, core rate for the scheduler (M2-b), e.g. 671875/7; frequencies in the output are then "
                    "to be read at that rate: compare_port.py --port-rate")
    ap.add_argument("--odds-trackoffset", type=int, default=None, help="override the trackoffset of channels 1/3/5 of the variant")
    ap.add_argument("--only", default=None, help="comma list of passes to render, e.g. P3b,P3c (default all four)")
    a = ap.parse_args()
    chans, odds_off, text = VARIANTS[a.variant]
    if a.odds_trackoffset is not None:
        odds_off = a.odds_trackoffset
        text += "; odds trackoffset %d" % odds_off
    if a.core_rate:
        text += "; core rate %s Hz" % a.core_rate
    only = set(a.only.split(",")) if a.only else None
    os.makedirs(a.outdir, exist_ok=True)
    tmp = os.path.join(a.outdir, "_tmp")
    os.makedirs(tmp, exist_ok=True)
    inwav = os.path.join(tmp, "smr_sig_main_mono.wav")
    write_wav_float_mono(inwav, C.main_input())
    lines = ["variant %s: %s; input gain %+g dB" % (a.variant, text, a.gain_db)]
    sat_total = 0
    for pid, stem, qpot, rot in PASSES:
        if only and pid not in only:
            continue
        out = render_pass(a.render, inwav, a.outdir, pid, stem, qpot, rot, chans, odds_off, a.gain_db, tmp, a.core_rate)
        m = re.search(r"audio saturation events (\d+), search-loop guard trips (\d+)", out)
        sat, trips = (int(m.group(1)), int(m.group(2))) if m else (-1, -1)
        under = re.search(r"underruns (\d+)", out)
        st = re.search(r"final state per channel \(1\.\.6\): (.*)", out)
        sat_total += max(sat, 0)
        lines.append("%s (Q pot %d, rotate %d): saturation events %d, loop-guard trips %d, underruns %s; %s"
                     % (pid, qpot, rot, sat, trips, under.group(1) if under else "?", st.group(1).strip() if st else "no state line"))
    os.remove(inwav)
    try:
        os.rmdir(tmp)
    except OSError:
        pass
    level = "warning" if sat_total else "notice"
    C.emit(level, "Port render, variant " + a.variant, lines)
    return 0


if __name__ == "__main__":
    sys.exit(main())
