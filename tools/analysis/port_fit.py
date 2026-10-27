#!/usr/bin/env python3
"""Milestone 3, third part: resolve the port-against-hardware gaps found with capture-1 (docs/open-questions.md M3-g to M3-i).

Usage: port_fit.py HWDIR --render PATH/smr_render [--task offset,qfit,tilt] [--json OUT.json]

Everything here is a measurement of the port's render against the hardware recordings, made by the same code as
compare_port.py (analyse_capture.response_spectrum and measure_peak). Nothing in the port is changed to fit the hardware.

Core rate. All renders use smr_render --core-rate 671875/7 (95,982.142857 Hz, the rate computed in docs/open-questions.md M2-b)
and the port's frequencies and widths are read at that rate (multiplied by 95982.142857 / 96000). Gains are not changed.

  offset  The odds Freq-jack offset (M3-g). Renders the Q max pass with the odds trackoffset (params.c trackoffset[0]) set to a
          series of counts and reports the mean frequency offset against the hardware, per jack. The evens jack stays at its
          default 0 and is the control. Shows which trackoffset reproduces the hardware, if any does.
  qfit    The Q setting (M3-h). Renders the main pass at a series of Q pot counts and reports, for each hardware peak, the count
          at which the port's full width at half power equals the hardware's, then the one count per pass that fits all six
          peaks best, with the width and gain left over. This says what the port needs to be given to look like the hardware;
          it does not change how the port maps the Q pot to Q.
  tilt    The high-frequency gain difference (M3-i). Takes the port-minus-hardware gain per third-octave band at Q min and
          fits four shapes to it: a first-order low-pass, a free two-corner shelf, the 50 us / 15 us de-emphasis shelf at its
          48 kHz corners and at twice those corners, and a straight line against log frequency. Then checks the P4 peaks
          above 5 kHz against each shape. The de-emphasis shapes are a hypothesis from codec.c (Reg 05 DEEMPH_48k, Reg 08
          "48k ADC/DAC") and the WM8731 curve as I recall it; the WM8731 datasheet is not in the project and has not been read.

Needs numpy and scipy.
"""
import argparse
import json
import os
import re
import shutil
import sys
import tempfile
from collections import OrderedDict

import numpy as np
from scipy.optimize import curve_fit

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyse_capture as A   # noqa: E402
import compare_port as K      # noqa: E402
import port_passes as P       # noqa: E402
import smrcap as C            # noqa: E402

RATE_NUM, RATE_DEN = 671875, 7
RATE = RATE_NUM / RATE_DEN
SCALE = RATE / C.FS
CORE_RATE = "%d/%d" % (RATE_NUM, RATE_DEN)
JACKS = K.JACKS

OFFSET_COUNTS = [11, 12, 13, 14, 15, 16, 17, 18, 19, 20]   # odds trackoffset; the firmware adds -11 (BASE_TRACKOFFSET), so 11 is a net 0
ODDS_OFF_FOR_FITS = P.ODDS_TRACKOFFSET                     # used by qfit and tilt (the stand-in from port_passes.py; changed after the offset task)
Q_COUNTS = [1000, 1300, 1500, 1700, 1800, 1850, 1900, 1950, 2000, 2048, 2400, 3000, 3600, 3800, 3900, 3950, 4000, 4040, 4070, 4095]
DEEMPH_TAU = (50e-6, 15e-6)   # the 50 us / 15 us pair of the standard (CD) de-emphasis; hypothesis, see the module text


class Renderer:
    def __init__(self, render, work, gain_db=-30.0):
        self.render, self.work, self.gain_db = render, work, gain_db
        self.inwav = os.path.join(work, "in_mono.wav")
        P.write_wav_float_mono(self.inwav, C.main_input())
        self.sat = 0
        self.n = 0

    def run(self, qpot, rot=0, odds_off=None, chans=None):
        self.n += 1
        out = os.path.join(self.work, "r%03d" % self.n)
        tmp = os.path.join(out, "_t")
        os.makedirs(tmp, exist_ok=True)
        stdout = P.render_pass(self.render, self.inwav, out, "X", "X", qpot, rot, chans or P.BASE_CHANS, odds_off, self.gain_db, tmp, CORE_RATE)
        m = re.search(r"audio saturation events (\d+), search-loop guard trips (\d+)", stdout)
        if m:
            self.sat += int(m.group(1)) + int(m.group(2))
        res = {"odds": C.read_wav(os.path.join(out, "X_in1_oddsOUT.wav")).x[:, 0].astype(np.float64),
               "evens": C.read_wav(os.path.join(out, "X_in2_evensOUT.wav")).x[:, 0].astype(np.float64)}
        shutil.rmtree(out)
        return res


def measure(y, cands_hz):
    """Measure the three peaks of one render near the hardware frequencies. Returns a list of dict(f, fwhm, gain) or None, read at the hardware rate."""
    Hfun, df = A.response_spectrum(y)
    wins = A.half_widths(list(cands_hz))
    rows = []
    for fc, hw in zip(cands_hz, wins):
        m = A.measure_peak(Hfun, df, fc / SCALE, hw / SCALE)
        rows.append(dict(f=m["f"] * SCALE, fwhm=m["fwhm"] * SCALE, gain=m["gain_db"]) if m["resolved"] else None)
    return rows


def hw_reference(hw_wavs):
    rows = K.peak_rows(hw_wavs)
    ref = {}
    for key, r in rows.items():
        rr = K.resolved_rows(r)
        if len(rr) == 3:
            ref[key] = rr
    return ref


# ---------------------------------------------------------------------------------------------------------------------
# offset

def task_offset(R, ref):
    cands = {j: [r["f_hz"] for r in ref["P3c_" + j]] for j in JACKS}
    rows = OrderedDict()
    lines = ["Port rendered at core rate %.6f Hz, read at that rate. Q max pass, three peaks per jack; port minus hardware in cent. "
             "Odds trackoffset set as listed (the firmware adds BASE_TRACKOFFSET -11, so 11 is a net 0); evens left at 0 as a control." % RATE]
    for T in OFFSET_COUNTS:
        res = R.run(4095, 0, odds_off=T)
        row = {}
        for j in JACKS:
            m = measure(res[j], cands[j])
            row[j] = [float(C.cents(x["f"], c)) for x, c in zip(m, cands[j]) if x]
        rows[T] = row
        lines.append("odds trackoffset %2d: odds mean %+.2f c (range %+.2f to %+.2f, %d peaks) | evens mean %+.2f c (range %+.2f to %+.2f)"
                     % (T, np.mean(row["odds"]), min(row["odds"]), max(row["odds"]), len(row["odds"]),
                        np.mean(row["evens"]), min(row["evens"]), max(row["evens"])))
    best = min(rows, key=lambda t: abs(np.mean(rows[t]["odds"])))
    lines.append("Closest to the hardware on the odds jack: trackoffset %d (mean %+.2f c)." % (best, np.mean(rows[best]["odds"])))
    return rows, [("notice", "M3-g odds Freq-jack offset sweep (core rate %.4f Hz)" % RATE, lines)]


# ---------------------------------------------------------------------------------------------------------------------
# qfit

def fit_one(qs, w, w_hw):
    """Q counts at which the port's width equals w_hw. The width is not a simple function of the Q count (filter.c: the first filter
    reaches its maximum Q at noon, the second filter only moves above 3900), so every interval of the sweep that brackets w_hw and
    has a real slope (more than 2 % change) gives one candidate, found by interpolating log width. Returns (list of counts, reason)."""
    qs, w = np.asarray(qs, float), np.asarray(w, float)
    ok = ~np.isnan(w)
    qs, w = qs[ok], w[ok]
    if len(qs) < 3:
        return [], "too few points"
    lw, t = np.log(w), np.log(w_hw)
    hits = []
    for i in range(len(qs) - 1):
        a, b = lw[i], lw[i + 1]
        if abs(a - b) < 0.02:
            continue
        if min(a, b) <= t <= max(a, b):
            hits.append(float(qs[i] + (t - a) / (b - a) * (qs[i + 1] - qs[i])))
    if hits:
        return hits, ""
    if t > lw.max():
        return [], "hardware wider than every port width in the sweep (widest %.3f Hz at %d)" % (w.max(), qs[int(np.argmax(w))])
    if t < lw.min():
        return [], "hardware narrower than every port width in the sweep (narrowest %.3f Hz at %d)" % (w.min(), qs[int(np.argmin(w))])
    return [], "hardware width lies on a flat part of the port's curve (no count is singled out)"


def task_qfit(R, ref):
    qs = Q_COUNTS
    meas = {j: {q: None for q in qs} for j in JACKS}
    for q in qs:
        res = R.run(q, 0, odds_off=ODDS_OFF_FOR_FITS)
        for j in JACKS:
            cands = [r["f_hz"] for r in ref["P3c_" + j]]
            meas[j][q] = measure(res[j], cands)
    out, ann = OrderedDict(), []
    lines = ["Port rendered at core rate %.6f Hz, odds trackoffset %d, Q pot swept over %s counts (0..4095; the pot count the port is given, "
             "nothing else changed). Widths are full width at half power in Hz, read at the hardware rate." % (RATE, ODDS_OFF_FOR_FITS, "/".join(map(str, qs)))]
    for j in JACKS:
        i = 1   # the middle peak of each jack
        lines.append("Port width (Hz) against Q count, %s jack %.1f Hz: " % (j, ref["P3c_" + j][i]["f_hz"]) +
                     ", ".join("%d: %.3f" % (q, meas[j][q][i]["fwhm"]) for q in qs if meas[j][q][i]))
    ann.append(("notice", "M3-h Q fit, port width against Q count (core rate %.4f Hz)" % RATE, lines))
    for pid in ("P3b", "P3c"):
        lines = []
        peaks = []   # (jack, idx, f_hw, w_hw, g_hw, widths over q, gains over q)
        for j in JACKS:
            for i, r in enumerate(ref["%s_%s" % (pid, j)]):
                w = [meas[j][q][i]["fwhm"] if meas[j][q][i] else np.nan for q in qs]
                g = [meas[j][q][i]["gain"] if meas[j][q][i] else np.nan for q in qs]
                peaks.append((j, i, r["f_hz"], r["fwhm_hz"], r["gain_db"], np.array(w), np.array(g)))
        lines.append("")
        lines.append("%s: hardware widths (Hz) and the Q count at which the port's width equals each:" % pid)
        for (j, i, f, whw, ghw, w, g) in peaks:
            hits, why = fit_one(qs, w, whw)
            lines.append("  %s %.1f Hz: hardware %.3f Hz; port at 2048 %.3f, at 4095 %.3f; equal width at Q count %s"
                         % (j, f, whw, w[qs.index(2048)], w[qs.index(4095)], "/".join("%.0f" % h for h in hits) if hits else "none (%s)" % why))
        grid = np.arange(qs[0], qs[-1] + 1, 1.0)
        cost = np.zeros_like(grid)
        for (j, i, f, whw, ghw, w, g) in peaks:
            ok = ~np.isnan(w)
            lw = np.interp(grid, np.array(qs, float)[ok], np.log(w[ok]))
            cost += (lw - np.log(whw)) ** 2
        qbest = float(grid[int(np.argmin(cost))])
        lines.append("  best single Q count for all %d %s peaks (least squares on log width): %.0f" % (len(peaks), pid, qbest))
        resid, gres = [], []
        for (j, i, f, whw, ghw, w, g) in peaks:
            ok = ~np.isnan(w)
            wq = float(np.exp(np.interp(qbest, np.array(qs, float)[ok], np.log(w[ok]))))
            gq = float(np.interp(qbest, np.array(qs, float)[ok], g[ok]))
            resid.append(100.0 * (wq / whw - 1.0))
            gres.append(gq - ghw)
            lines.append("    %s %.1f Hz at %.0f: width %+.1f %%, gain port minus hardware %+.2f dB (raw, the absolute level offset is not removed)" % (j, f, qbest, resid[-1], gres[-1]))
        nin = sum(abs(r) <= K.TOL_WIDTH_PCT for r in resid)
        lines.append("  at %.0f: %d of %d widths inside %.0f %%; gain port minus hardware spread %.2f dB (max minus min)" % (qbest, nin, len(peaks), K.TOL_WIDTH_PCT, max(gres) - min(gres)))
        out[pid] = dict(best_q=qbest, width_resid_pct=resid, gain_diff_db=gres)
        ann.append(("notice", "M3-h Q fit, %s peaks (core rate %.4f Hz)" % (pid, RATE), lines))
    return out, ann


# ---------------------------------------------------------------------------------------------------------------------
# tilt

def lp1(f, c, fc):
    return c + 10.0 * np.log10(1.0 + (f / fc) ** 2)


def shelf(f, c, fp, fz):
    return c + 10.0 * np.log10((1.0 + (f / fp) ** 2) / (1.0 + (f / fz) ** 2))


def deemph_shape(f, k):
    fp = 1.0 / (2 * np.pi * DEEMPH_TAU[0]) * k
    fz = 1.0 / (2 * np.pi * DEEMPH_TAU[1]) * k
    return 10.0 * np.log10((1.0 + (f / fp) ** 2) / (1.0 + (f / fz) ** 2))


def task_tilt(R, hw_wavs, ref):
    ann, out = [], OrderedDict()
    res_q = R.run(0, 0, odds_off=ODDS_OFF_FOR_FITS)
    lines = ["Port rendered at core rate %.6f Hz, Q min pass; per third-octave band, port minus hardware in dB (output power over input power), "
             "only bands 10 dB above the hardware floor. d(f) = shape(f) + c, c a free constant per jack (the absolute level is unknown, M1-n)." % RATE]
    shapes = {}
    for j, jn in ((1, "odds"), (2, "evens")):
        if ("P3a", j) not in hw_wavs:
            lines.append("%s: no P3a recording" % jn)
            continue
        wr = K.whole_response(hw_wavs[("P3a", j)].x[:, 0], res_q[jn])
        if wr is None:
            lines.append("%s: no usable band" % jn)
            continue
        idx = [i for i, g in enumerate(wr["gated"]) if g]
        f = np.array([wr["centres"][i] for i in idx])
        d = np.array([wr["g_port"][i] - wr["g_hw"][i] for i in idx])
        fits = OrderedDict()
        # 1. first-order low-pass with a free corner
        p, _ = curve_fit(lp1, f, d, p0=[-1.0, 8000.0], bounds=([-60, 500], [60, 100000]))
        fits["first-order low-pass (free corner)"] = (lambda x, p=p: lp1(x, *p), "c %+.2f dB, corner %.0f Hz" % (p[0], p[1]))
        # 2. free two-corner shelf
        try:
            p2, _ = curve_fit(shelf, f, d, p0=[-1.0, 6000.0, 20000.0], bounds=([-60, 500, 500], [60, 100000, 400000]), maxfev=20000)
            fits["two-corner shelf (free corners)"] = (lambda x, p=p2: shelf(x, *p), "c %+.2f dB, pole %.0f Hz, zero %.0f Hz" % tuple(p2))
        except RuntimeError:
            fits["two-corner shelf (free corners)"] = (None, "fit did not converge")
        # 3 and 4. 50/15 us shelf at the 48 kHz corners and at twice those corners (constant fitted only)
        for k, name in ((1, "50/15 us shelf at 48 kHz corners (3183 Hz, 10610 Hz)"), (2, "50/15 us shelf at twice those (6366 Hz, 21221 Hz)")):
            s = deemph_shape(f, k)
            c = float(np.mean(d - s))
            fits[name] = ((lambda x, c=c, k=k: c + deemph_shape(x, k)), "c %+.2f dB" % c)
        # 5. straight line in dB against log2 f
        a = np.polyfit(np.log2(f), d, 1)
        fits["straight line against log f"] = ((lambda x, a=a: np.polyval(a, np.log2(x))), "%+.2f dB per octave, c %+.2f dB at 1 Hz" % (a[0], a[1]))
        lines.append("")
        lines.append("%s jack: %d bands from %.0f Hz to %.0f Hz, d from %+.2f to %+.2f dB" % (jn, len(f), f[0], f[-1], d.min(), d.max()))
        for name, (fn, txt) in fits.items():
            if fn is None:
                lines.append("  %-58s %s" % (name, txt))
                continue
            r = d - fn(f)
            lines.append("  %-58s rms %.2f dB, worst %+.2f dB at %.0f Hz; %s" % (name, float(np.sqrt(np.mean(r ** 2))), r[int(np.argmax(np.abs(r)))], f[int(np.argmax(np.abs(r)))], txt))
        shapes[jn] = fits
        out[jn] = dict(f=f.tolist(), d=d.tolist())
    ann.append(("notice", "M3-i HF gain tilt, Q min whole response (core rate %.4f Hz)" % RATE, lines))

    # P4 peaks against the shapes
    res_p4 = R.run(4095, 10, odds_off=ODDS_OFF_FOR_FITS)
    lines = ["P4 (Q max, rotate 10) peaks, port minus hardware gain in dB, raw. For each shape, one constant per jack is fitted to the P4 peaks below 1 kHz-or-above-5 kHz "
             "as a group (median), then the residual per peak is listed. A shape that explains the tilt leaves small residuals at the high peaks."]
    for jn in JACKS:
        r4 = ref["P4_" + jn]
        cands = [r["f_hz"] for r in r4]
        m = measure(res_p4[jn], cands)
        pk = [(r["f_hz"], x["gain"] - r["gain_db"], 100.0 * (x["fwhm"] / r["fwhm_hz"] - 1.0)) for r, x in zip(r4, m) if x]
        lines.append("")
        lines.append("%s jack: " % jn + " | ".join("%.1f Hz: gain %+.2f dB, width %+.1f %%" % p for p in pk))
        hf = [p for p in pk if p[0] > 5000.0]
        if jn not in shapes or len(hf) < 1:
            continue
        for name, (fn, txt) in shapes[jn].items():
            if fn is None:
                continue
            s = np.array([fn(p[0]) for p in pk])
            g = np.array([p[1] for p in pk])
            ref_idx = [i for i, p in enumerate(pk) if p[0] > 5000.0]
            k = float(np.median((g - s)[ref_idx]))
            lines.append("  %-58s residual (observed minus shape minus %+.2f dB): " % (name, k) + ", ".join("%.1f Hz %+.2f" % (pk[i][0], g[i] - s[i] - k) for i in range(len(pk))))
    ann.append(("notice", "M3-i HF gain tilt, P4 peaks against the shapes", lines))
    return out, ann


# ---------------------------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("hwdir")
    ap.add_argument("--render", required=True)
    ap.add_argument("--task", default="offset,qfit,tilt")
    ap.add_argument("--json")
    ap.add_argument("--odds-trackoffset", type=int, default=None, help="odds trackoffset used by qfit and tilt (default: port_passes.ODDS_TRACKOFFSET)")
    a = ap.parse_args()
    global ODDS_OFF_FOR_FITS
    if a.odds_trackoffset is not None:
        ODDS_OFF_FOR_FITS = a.odds_trackoffset
    tasks = a.task.split(",")
    _, hw_wavs = A.load_capture(a.hwdir, A.Report())
    ref = hw_reference(hw_wavs)
    need = ["P3b_odds", "P3b_evens", "P3c_odds", "P3c_evens", "P4_odds", "P4_evens"]
    miss = [k for k in need if k not in ref]
    if miss:
        C.emit("error", "port_fit: hardware reference incomplete", ["fewer than three resolved peaks for: " + ", ".join(miss)])
        return 1
    work = tempfile.mkdtemp(prefix="portfit_")
    R = Renderer(a.render, work)
    result = OrderedDict()
    try:
        for t in tasks:
            fn = {"offset": lambda: task_offset(R, ref), "qfit": lambda: task_qfit(R, ref), "tilt": lambda: task_tilt(R, hw_wavs, ref)}[t]
            data, ann = fn()
            result[t] = data
            for level, topic, lines in ann:
                C.emit(level, topic, lines)
        C.emit("warning" if R.sat else "notice", "port_fit renders", ["%d renders; audio saturation events plus search-loop guard trips over all of them: %d" % (R.n, R.sat)])
    finally:
        shutil.rmtree(work, ignore_errors=True)
    if a.json:
        with open(a.json, "w") as f:
            json.dump(result, f, indent=1, default=float)
    return 0


if __name__ == "__main__":
    sys.exit(main())
