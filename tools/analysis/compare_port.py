#!/usr/bin/env python3
"""Milestone 3: compare the port's render with the hardware capture, measured by the same code.

Usage: compare_port.py HWDIR PORTDIR [--label NAME] [--json OUT.json]

HWDIR is the capture (the 14 WAV files of capture-1). PORTDIR holds the port's render in the same layout
(tools/analysis/port_passes.py). Both are measured by tools/analysis/analyse_capture.py (the same H(f) = Y(f) / X(f)
method, same peak search, same half-power rules), then compared:

  Per resonance (P3b, P3c, P4; three per jack): centre frequency difference in cents, full width at half power as a
  percentage, peak gain difference in dB.
    Tolerances (docs/status.md, agreed 2026-10-25 as a starting point): 1 cent, 10 % width, 0.5 dB gain.
    Gain: the absolute level of the chain (Scarlett, SMR input and output stages, the port's placeholder level scale
    M1-n) is not known, so one common gain offset per jack is removed (the median over all compared peaks of that jack)
    and the residual is judged. Without the loopback pass P5 nothing better is possible.
  Whole response at Q min (P3a): no distinct peaks, so output power over input power in 1/3-octave bands, only where the
    hardware recording is at least 10 dB above its own noise floor; one common offset per jack removed; deviation per band
    judged against 0.5 dB.

It measures and states which numbers are inside the tolerances. It does not decide what a miss means.
Needs numpy and scipy.
"""
import argparse
import json
import os
import sys
from collections import OrderedDict

import numpy as np
from scipy.signal import welch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyse_capture as A   # noqa: E402
import smrcap as C            # noqa: E402

FS = C.FS
TOL_CENT, TOL_WIDTH_PCT, TOL_GAIN_DB = 1.0, 10.0, 0.5
SNR_GATE_DB = 10.0
PASSES_PEAKS = ("P3b", "P3c", "P4")
JACKS = ("odds", "evens")
JACK_NO = {"odds": 1, "evens": 2}


def peak_rows(wavs):
    """Run the capture analysis' resonance measurement on a set of recordings. Returns {key: [row, ...]}."""
    rep = A.Report()
    A.resonances(wavs, rep)
    return rep.data["resonances"]


def resolved_rows(rows):
    return [r for r in rows if r.get("resolved")]


def compare_peaks(hw_rows, port_rows):
    """Pair the peaks of each pass and jack by frequency order. Returns (list of dicts, notes)."""
    out, notes = [], []
    for pid in PASSES_PEAKS:
        for jack in JACKS:
            key = "%s_%s" % (pid, jack)
            h, p = resolved_rows(hw_rows.get(key, [])), resolved_rows(port_rows.get(key, []))
            if len(h) != 3 or len(p) != 3:
                notes.append("%s: hardware has %d and port has %d measured peaks (need 3 and 3); not compared" % (key, len(h), len(p)))
                continue
            for i, (a, b) in enumerate(zip(h, p)):
                out.append(dict(pass_=pid, jack=jack, idx=i, f_hw=a["f_hz"], f_port=b["f_hz"],
                                dcent=float(C.cents(b["f_hz"], a["f_hz"])),
                                fwhm_hw=a["fwhm_hz"], fwhm_port=b["fwhm_hz"],
                                dwidth_pct=100.0 * (b["fwhm_hz"] / a["fwhm_hz"] - 1.0),
                                gain_hw=a["gain_db"], gain_port=b["gain_db"],
                                dgain=b["gain_db"] - a["gain_db"]))
    return out, notes


def gain_residuals(peaks):
    """Remove one common gain offset per jack (median). Adds 'rgain' to each peak; returns the offsets."""
    offs = {}
    for jack in JACKS:
        sel = [p for p in peaks if p["jack"] == jack]
        offs[jack] = float(np.median([p["dgain"] for p in sel])) if sel else None
    for p in peaks:
        p["rgain"] = p["dgain"] - offs[p["jack"]]
    return offs


# ---------------------------------------------------------------------------------------------------------------------
# Whole response at Q min

def third_octave_bands(fmin=31.5, fmax=16000.0):
    """Centres 1000 * 2^(k/3), from about 31.5 Hz up to 16 kHz."""
    c = 1000.0 * 2.0 ** (np.arange(-15, 14) / 3.0)
    return c[(c >= fmin * 0.98) & (c <= fmax)]


def band_power(f, P, centres):
    out = []
    df = f[1] - f[0]
    for c in centres:
        m = (f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))
        out.append(float(np.sum(P[m]) * df))
    return np.array(out)


def whole_response(hw_y, port_y):
    """Band gains (output power over input power, dB) for one jack at Q min. Returns dict or None."""
    n = min(len(hw_y), len(port_y), int(C.MAIN["total"] * FS))   # the same stretch of time for input, hardware and port
    x = C.main_input()[:n]
    nper = 1 << 16
    centres = third_octave_bands()

    def psd(sig):
        return welch(np.asarray(sig, dtype=np.float64), fs=FS, nperseg=nper, noverlap=nper // 2)

    f, Px = psd(x)
    bx = band_power(f, Px, centres)
    res = {}
    for name, y in (("hw", hw_y), ("port", port_y)):
        yy = np.asarray(y[:n], dtype=np.float64)
        f, Py = psd(yy)
        fl, Pf = welch(np.asarray(y[int(0.3 * FS):int(1.7 * FS)], dtype=np.float64), fs=FS, nperseg=1 << 14)   # silent start: the floor
        by = band_power(f, Py, centres)
        bf = band_power(fl, Pf, centres)
        res[name] = (by, bf)
    by_hw, bf_hw = res["hw"]
    by_pt, bf_pt = res["port"]
    ok = (by_hw >= bf_hw * 10 ** (SNR_GATE_DB / 10.0)) & (by_pt > bf_pt)
    g_hw = 10 * np.log10(np.maximum(by_hw - bf_hw, 1e-30) / bx)
    g_pt = 10 * np.log10(np.maximum(by_pt - bf_pt, 1e-30) / bx)
    d = g_pt - g_hw
    if not np.any(ok):
        return None
    off = float(np.median(d[ok]))
    dev = d - off
    return dict(centres=[float(c) for c in centres], gated=[bool(v) for v in ok], g_hw=[float(v) for v in g_hw], g_port=[float(v) for v in g_pt],
                offset_db=off, dev_db=[float(v) for v in dev])


# ---------------------------------------------------------------------------------------------------------------------

def fmt_peak(p):
    return "%.1f Hz: %+.2f c, width %+.1f %%, gain %+.2f dB" % (p["f_hw"], p["dcent"], p["dwidth_pct"], p["rgain"])


def compare(hw_wavs, port_wavs, label="port"):
    """The comparison itself. hw_wavs and port_wavs are {(pass, jack): Wav}. Returns (result dict, list of (level, topic, lines))."""
    ann = []
    hw_rows, port_rows = peak_rows(hw_wavs), peak_rows(port_wavs)
    peaks, notes = compare_peaks(hw_rows, port_rows)
    offs = gain_residuals(peaks) if peaks else {j: None for j in JACKS}
    result = OrderedDict(label=label, peaks=peaks, gain_offset_db=offs, notes=notes)

    lines = ["Port minus hardware, measured by the same code. Tolerances: %.0f cent, %.0f %% width, %.1f dB gain." % (TOL_CENT, TOL_WIDTH_PCT, TOL_GAIN_DB),
             "Gain is judged after removing one common offset per jack (the absolute level is not known, M1-n): odds %s dB, evens %s dB (median over %d peaks)."
             % tuple([("%+.2f" % offs["odds"]) if offs["odds"] is not None else "n/a", ("%+.2f" % offs["evens"]) if offs["evens"] is not None else "n/a", len(peaks) // 2 if peaks else 0])]
    for n in notes:
        lines.append("NOTE: " + n)
    for pid in PASSES_PEAKS:
        for jack in JACKS:
            sel = [p for p in peaks if p["pass_"] == pid and p["jack"] == jack]
            if sel:
                lines.append("%s %s: " % (pid, jack) + " | ".join(fmt_peak(p) for p in sel))
    n = len(peaks)
    if n:
        okf = sum(abs(p["dcent"]) <= TOL_CENT for p in peaks)
        okw = sum(abs(p["dwidth_pct"]) <= TOL_WIDTH_PCT for p in peaks)
        okg = sum(abs(p["rgain"]) <= TOL_GAIN_DB for p in peaks)
        lines.append("Inside tolerance, of %d peaks: frequency %d, width %d, gain %d." % (n, okf, okw, okg))
        for jack in JACKS:
            dc = [p["dcent"] for p in peaks if p["jack"] == jack]
            lines.append("%s jack frequency offset (port minus hardware): mean %+.2f c, range %+.2f to %+.2f c over %d peaks." % (jack, np.mean(dc), min(dc), max(dc), len(dc)))
        result["inside"] = dict(n=n, frequency=int(okf), width=int(okw), gain=int(okg))
        level = "notice" if (okf == n and okw == n and okg == n) else "warning"
    else:
        level = "warning"
    ann.append((level, "Port against hardware: resonances (%s)" % label, lines))

    # whole response at Q min
    wr = OrderedDict()
    wl = []
    wlevel = "notice"
    for jack in JACKS:
        key = ("P3a", JACK_NO[jack])
        if key not in hw_wavs or key not in port_wavs:
            wl.append("P3a %s: a recording is missing; not compared" % jack)
            wlevel = "warning"
            continue
        r = whole_response(hw_wavs[key].x[:, 0], port_wavs[key].x[:, 0])
        if r is None:
            wl.append("P3a %s: no band is 10 dB above the hardware noise floor; not compared" % jack)
            wlevel = "warning"
            continue
        wr[jack] = r
        idx = [i for i, g in enumerate(r["gated"]) if g]
        dev = np.array([r["dev_db"][i] for i in idx])
        worst = int(np.argmax(np.abs(dev)))
        nbad = int(np.sum(np.abs(dev) > TOL_GAIN_DB))
        wl.append("P3a %s (Q min): %d bands (1/3 octave, %.0f Hz to %.0f Hz) are 10 dB above the hardware floor; common offset %+.2f dB; "
                  "largest deviation %+.2f dB at %.0f Hz; rms deviation %.2f dB; %d of %d bands outside %.1f dB."
                  % (jack, len(idx), r["centres"][idx[0]], r["centres"][idx[-1]], r["offset_db"], dev[worst], r["centres"][idx[worst]],
                     float(np.sqrt(np.mean(dev ** 2))), nbad, len(idx), TOL_GAIN_DB))
        if nbad:
            wlevel = "warning"
            bad = [i for i in idx if abs(r["dev_db"][i]) > TOL_GAIN_DB]
            wl.append("  bands outside tolerance (centre Hz: deviation dB): " + ", ".join("%.0f: %+.2f" % (r["centres"][i], r["dev_db"][i]) for i in bad[:20]))
    result["q_min_response"] = wr
    ann.append((wlevel, "Port against hardware: whole response at Q min (%s)" % label, wl))
    return result, ann


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("hwdir")
    ap.add_argument("portdir")
    ap.add_argument("--label", default="port")
    ap.add_argument("--json")
    a = ap.parse_args()
    _, hw_wavs = A.load_capture(a.hwdir, A.Report())
    _, port_wavs = A.load_capture(a.portdir, A.Report())
    result, ann = compare(hw_wavs, port_wavs, a.label)
    for level, topic, lines in ann:
        C.emit(level, topic, lines)
    if a.json:
        with open(a.json, "w") as f:
            json.dump(result, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
