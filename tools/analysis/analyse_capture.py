#!/usr/bin/env python3
"""Milestone 3 (Tier A) analysis of the hardware capture recorded through a real SMR.

Usage: analyse_capture.py CAPDIR [--json OUT.json]

Measures only. It does not compare with the port yet (that is a later step) and it states no pass/fail against the
agreed tolerances. Everything it prints is a measurement from the recordings, with the assumptions named.
Needs numpy and scipy.
"""
import argparse
import json
import os
import sys
from collections import OrderedDict

import numpy as np
from scipy.signal import find_peaks, welch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smrcap as C   # noqa: E402

FS = C.FS
SEVERITY = {"notice": 0, "warning": 1, "error": 2}


def r1(v):
    return None if v is None else round(float(v), 3)


def rms_db(y):
    y = np.asarray(y, dtype=np.float64)
    return 10.0 * np.log10(np.mean(y * y) + 1e-18)


def block_db(y, blk):
    n = len(y) // blk
    return 10.0 * np.log10(np.mean(np.asarray(y[:n * blk], dtype=np.float64).reshape(n, blk) ** 2, axis=1) + 1e-18)


class Report:
    """Collects findings per topic. One annotation per topic (a step shows at most 10 per level)."""

    def __init__(self):
        self.topics = OrderedDict()
        self.data = OrderedDict()

    def add(self, topic, level, line):
        self.topics.setdefault(topic, []).append((level, line))

    def flush(self):
        for topic, items in self.topics.items():
            worst = max(items, key=lambda it: SEVERITY[it[0]])[0]
            C.emit(worst, topic, [("" if lv == "notice" else lv.upper() + ": ") + ln for lv, ln in items])

    def worst(self):
        return max((SEVERITY[lv] for items in self.topics.values() for lv, _ in items), default=0)


# ---------------------------------------------------------------------------------------------------------------------
# Loading and inventory

def load_capture(capdir, rep):
    files, wavs = {}, {}
    for name in sorted(os.listdir(capdir)):
        if not name.lower().endswith(".wav") or name.startswith("."):
            continue
        key = C.classify(name)
        if key is None or key[1] is None or key[0] not in C.PASSES:
            rep.add("Inventory", "warning", "ignored file with an unrecognised name: " + name)
            continue
        if key in files:
            rep.add("Inventory", "error", "two files for %s %s: %s and %s" % (key[0], C.JACK_NAME[key[1]], files[key], name))
            continue
        files[key] = name
    for key, name in files.items():
        try:
            wavs[key] = C.read_wav(os.path.join(capdir, name))
        except Exception as e:   # noqa: BLE001
            rep.add("Inventory", "error", "cannot read %s: %s" % (name, e))
    return files, wavs


def inventory(files, wavs, rep):
    inv = OrderedDict()
    lines = 0
    for key in sorted(files, key=lambda k: (k[0], k[1])):
        if key not in wavs:
            continue
        w, name = wavs[key], files[key]
        pid, jack = key
        kind = C.PASSES[pid]["kind"]
        label = "%s %s (%s)" % (pid, C.JACK_NAME[jack], name)
        row = dict(file=name, sr=w.sr, bits=w.bits, channels=w.x.shape[1])
        if w.sr != FS:
            rep.add("Inventory", "error", "%s: sample rate %d Hz, expected %d" % (label, w.sr, FS))
        if w.bits != 24:
            rep.add("Inventory", "warning", "%s: %d-bit, expected 24" % (label, w.bits))
        if w.x.shape[1] != 1:
            rep.add("Inventory", "warning", "%s: %d channels, expected mono (analysis uses channel 1)" % (label, w.x.shape[1]))
        y = w.x[:, 0].astype(np.float64)
        dur = len(y) / w.sr
        peak = np.max(np.abs(y))
        clip = int(np.sum(np.abs(y) >= 0.9999))
        row.update(duration_s=r1(dur), peak_dbfs=r1(20 * np.log10(peak + 1e-12)), rms_dbfs=r1(rms_db(y)),
                   clipped_samples=clip, dc=float(np.mean(y)))
        need = {"noise": 30.0, "sweep": C.SWEEP["start"] + C.SWEEP["dur"] + 1.0,
                "main": C.MAIN["step_t0"] + 6 * 7 + C.MAIN["step_noise"] + 1.0, "loopback": 100.0}[kind]
        if dur < need:
            rep.add("Inventory", "warning", "%s: %.1f s long, shorter than the %.1f s needed to hold the whole signal" % (label, dur, need))
        if clip:
            rep.add("Inventory", "error", "%s: %d clipped samples" % (label, clip))
        if peak < 10 ** (-100 / 20):
            rep.add("Inventory", "error", "%s: silent (peak below -100 dBFS)" % label)
        elif peak > 10 ** (-1 / 20):
            rep.add("Inventory", "warning", "%s: peak within 1 dB of full scale" % label)
        inv["%s_%s" % (pid, C.JACK_NAME[jack])] = row
        lines += 1
    present = {k[0] for k in files}
    for pid, spec in C.PASSES.items():
        for jack in ((1,) if pid == "P5" else (1, 2)):
            if (pid, jack) not in files:
                lvl = "notice" if spec.get("optional") else "warning"
                rep.add("Inventory", lvl, "missing: %s %s%s" % (pid, C.JACK_NAME[jack], " (optional)" if spec.get("optional") else ""))
    rep.add("Inventory", "notice", "%d files read; all recordings are used as mono, channel 1; no normalising applied" % lines)
    rep.data["inventory"] = inv


# ---------------------------------------------------------------------------------------------------------------------
# Alignment

def alignment(wavs, rep):
    out = OrderedDict()
    for pid in ("P3a", "P3b", "P3c", "P4"):
        for jack in (1, 2):
            if (pid, jack) not in wavs:
                continue
            y = wavs[(pid, jack)].x[:, 0]
            blk = int(0.05 * FS)
            e = block_db(y, blk)
            floor = np.median(e[int(0.2 / 0.05):int(1.8 / 0.05)])
            above = e > floor + 10.0
            run = above[:-2] & above[1:-1] & above[2:]
            idx = np.where(run & (np.arange(len(run)) >= int(1.0 / 0.05)))[0]
            if len(idx) == 0:
                rep.add("Alignment", "warning", "%s %s: noise onset not found" % (pid, C.JACK_NAME[jack]))
                continue
            onset = idx[0] * 0.05
            off = onset - C.MAIN["noiseA"][0]
            out["%s_%s" % (pid, C.JACK_NAME[jack])] = r1(off)
            if abs(off) > 0.3 and pid in ("P3a", "P3b"):   # high-Q passes build up slowly, so their onset is not a timing test
                rep.add("Alignment", "warning", "%s %s: noise starts at %.2f s, expected 2.00 s (offset %+.2f s)" % (pid, C.JACK_NAME[jack], onset, off))
    if out:
        vals = list(out.values())
        rep.add("Alignment", "notice", "main files: time the output rises 10 dB above its floor minus 2.00 s (50 ms resolution): min %+.2f, max %+.2f s over %d files. "
                "Only P3a and P3b (fast onset) are judged; high-Q passes build up slowly. Recordings are assumed to start at the project start; sweeps are not separately aligned." % (min(vals), max(vals), len(vals)))
    rep.data["alignment_offsets_s"] = out


# ---------------------------------------------------------------------------------------------------------------------
# Channel mapping (M1-m) and coarse sweep peaks

def sweep_peak(y):
    L = C.SWEEP["dur"] / np.log(C.SWEEP["f2"] / C.SWEEP["f1"])
    blk = int(0.05 * FS)
    n = len(y) // blk
    e = np.sqrt(np.mean(np.asarray(y[:n * blk], dtype=np.float64).reshape(n, blk) ** 2, axis=1))
    t = (np.arange(n) + 0.5) * 0.05
    ok = (t > C.SWEEP["start"]) & (t < C.SWEEP["start"] + C.SWEEP["dur"])
    i = int(np.argmax(np.where(ok, e, 0.0)))
    return C.SWEEP["f1"] * np.exp((t[i] - C.SWEEP["start"]) / L), 20 * np.log10(e[i] + 1e-12)


def mapping(wavs, rep):
    res = OrderedDict()
    for pid, slider in (("P1", 1), ("P2", 2)):
        if (pid, 1) not in wavs or (pid, 2) not in wavs:
            rep.add("Output mapping (M1-m)", "warning", "%s: need both jacks to decide which jack slider %d uses" % (pid, slider))
            continue
        y1, y2 = wavs[(pid, 1)].x[:, 0], wavs[(pid, 2)].x[:, 0]
        a = slice(int(C.SWEEP["start"] * FS), int((C.SWEEP["start"] + C.SWEEP["dur"]) * FS))
        l1, l2 = rms_db(y1[a]), rms_db(y2[a])
        jack = 1 if l1 > l2 else 2
        f, lvl = sweep_peak((y1, y2)[jack - 1])
        res[pid] = dict(slider=slider, odds_rms_dbfs=r1(l1), evens_rms_dbfs=r1(l2), loud_jack=C.JACK_NAME[jack],
                        separation_db=r1(abs(l1 - l2)), coarse_peak_hz=r1(f))
        rep.add("Output mapping (M1-m)", "notice",
                "%s (only slider %d up): odds jack %.1f dBFS rms, evens jack %.1f dBFS rms, so slider %d leaves by the %s jack "
                "(separation %.0f dB). Sweep peak on that jack about %.0f Hz (coarse, +-3%%)." % (pid, slider, l1, l2, slider, C.JACK_NAME[jack].upper(), abs(l1 - l2), f))
        if abs(l1 - l2) < 20:
            rep.add("Output mapping (M1-m)", "warning", "%s: the two jacks differ by only %.1f dB; mapping not clear" % (pid, abs(l1 - l2)))
    if "P1" in res and "P2" in res:
        # Source (filter.c, docs/architecture.md section 3.1): channels 1,3,5 leave by EVENS OUT, 2,4,6 by ODDS OUT.
        src = {1: "evens", 2: "odds"}
        got = {res["P1"]["slider"]: res["P1"]["loud_jack"], res["P2"]["slider"]: res["P2"]["loud_jack"]}
        same = all(got[s] == src[s] for s in (1, 2))
        res["matches_source"] = same
        if same:
            rep.add("Output mapping (M1-m)", "notice", "Matches the source and manual mapping (channels 1,3,5 by EVENS OUT).")
        elif got[1] == got[2]:
            rep.add("Output mapping (M1-m)", "warning", "Both sliders appear on the same jack; check cabling.")
        else:
            rep.add("Output mapping (M1-m)", "warning", "DIFFERS from the source and manual, which put channels 1,3,5 on the EVENS OUT jack. "
                    "Measured: slider 1 on %s, slider 2 on %s (assumes cabling and Logic inputs as the capture sheet says)." % (got[1].upper(), got[2].upper()))
    rep.data["mapping"] = res


# ---------------------------------------------------------------------------------------------------------------------
# Noise source, nothing patched in (P0)

def noise_source(wavs, rep):
    res = OrderedDict()
    centres = [62.5 * 2 ** k for k in range(9)]   # 62.5 Hz .. 16 kHz
    ys = {}
    for jack in (1, 2):
        if ("P0", jack) not in wavs:
            continue
        y = wavs[("P0", jack)].x[:, 0].astype(np.float64)[int(2 * FS):]
        ys[jack] = y
        f, P = welch(y, fs=FS, nperseg=2 ** 15)
        df = f[1] - f[0]
        bands = []
        for c in centres:
            m = (f >= c / np.sqrt(2)) & (f < c * np.sqrt(2))
            bands.append(r1(10 * np.log10(np.sum(P[m]) * df + 1e-18)))
        res[C.JACK_NAME[jack]] = dict(rms_dbfs=r1(rms_db(y)), peak_dbfs=r1(20 * np.log10(np.max(np.abs(y)))), octave_bands_dbfs=bands)
        rep.add("Noise source, nothing patched (M1-n)", "notice", "%s jack: rms %.1f dBFS, peak %.1f dBFS (after the first 2 s). Octave bands %s Hz: %s dBFS"
                % (C.JACK_NAME[jack], rms_db(y), 20 * np.log10(np.max(np.abs(y))), "/".join("%g" % c for c in centres[::2]), "/".join("%.0f" % b for b in res[C.JACK_NAME[jack]]["octave_bands_dbfs"][::2])))
    if len(ys) == 2:
        n = min(len(ys[1]), len(ys[2]))
        cc = float(np.corrcoef(ys[1][:n], ys[2][:n])[0, 1])
        res["correlation_between_jacks"] = r1(cc)
        rep.add("Noise source, nothing patched (M1-n)", "notice", "correlation between the two jacks: %.3f (1 = the same noise on both)" % cc)
    rep.data["noise_source"] = res


# ---------------------------------------------------------------------------------------------------------------------
# Resonances: centre frequency, width and peak gain, from the response H(f) = Y(f) / X(f)
#
# X is the exact test signal smr_sig_main.wav (regenerated from tools/capture/gen_signals.py, fixed seed) and Y is the
# whole recorded file. The signal is white noise and the system is treated as linear, so H = Y X* / (|X|^2 + eps) is the
# response of Scarlett output + SMR + Scarlett input. Time alignment is not needed for |H|. The single-sample impulses in
# the file are NOT analysed on their own: their rings are below the SMR's noise floor (see README).
#
# Per resonance (a peak of |H|^2): centre = power-weighted mean frequency between the half-power points, FWHM = distance
# between the half-power points, peak gain = |H|^2 at the top. "T60-equivalent" = 2.199 / FWHM, the ring time a single
# exponential with that width would have. The SMR in Two-Pass mode is not a single pole, so that number is only a
# model-free way to compare hardware and port, measured the same way on both.

NOISE_A = (3.0, 21.5)   # seconds of section A used only to list the candidate peaks
_X_CACHE = {}


def find_modes(y, max_modes=8, thr_db=50.0, fmin=20.0):
    """Strong peaks of the averaged power spectrum. Returns [(f_hz, level_db_above_median)] sorted by f."""
    f, P = welch(np.asarray(y, dtype=np.float64), fs=FS, nperseg=2 ** 18, noverlap=2 ** 17)
    Pdb = 10 * np.log10(P + 1e-30)
    sel = (f >= fmin) & (f <= FS * 0.45)
    floor = float(np.median(Pdb[sel]))
    idx, _ = find_peaks(np.where(sel, Pdb, -300.0), height=floor + thr_db, prominence=30.0, distance=8)
    order = sorted(idx, key=lambda i: -Pdb[i])[:max_modes]
    modes = []
    for i in sorted(order, key=lambda i: f[i]):
        if modes and f[i] / modes[-1][0] < 1.015:
            if Pdb[i] - floor > modes[-1][1]:
                modes[-1] = (float(f[i]), float(Pdb[i] - floor))
            continue
        modes.append((float(f[i]), float(Pdb[i] - floor)))
    return modes


def input_spectrum(N):
    if N not in _X_CACHE:
        _X_CACHE.clear()
        _X_CACHE[N] = np.fft.rfft(C.main_input(), N)
    return _X_CACHE[N]


def response_spectrum(y):
    """Returns (H slice function, df) for one recording. H is computed only for the bins asked for."""
    y = np.asarray(y, dtype=np.float64)
    N = 1 << int(np.ceil(np.log2(max(len(y), int(C.MAIN["total"] * FS)) + 1)))
    X = input_spectrum(N)
    Y = np.fft.rfft(y, N)
    eps = 1e-3 * float(np.mean(np.abs(X[1000:200000]) ** 2))
    df = FS / N

    def H(f_lo, f_hi):
        k0, k1 = max(1, int(f_lo / df)), min(len(X) - 1, int(f_hi / df) + 1)
        # Smooth the cross-spectrum and the input power over 9 bins (about 0.05 Hz) before dividing: a bin where the random
        # noise input happens to be weak would otherwise give a dip or spike. 0.05 Hz is far below any resonance width here.
        box = np.ones(9) / 9.0
        num = np.convolve(Y[k0:k1] * np.conj(X[k0:k1]), box, mode="same")
        den = np.convolve(np.abs(X[k0:k1]) ** 2, box, mode="same")
        return k0, num / (den + eps)
    return H, df


def measure_peak(Hfun, df, fc, hw):
    """Half-power analysis of the response peak near fc, searching fc +- hw. Returns dict or dict(resolved=False, why)."""
    k0, H = Hfun(fc - hw, fc + hw)
    P = np.abs(H) ** 2
    i0 = int(np.argmax(P))
    top = P[i0]
    f_top = (k0 + i0) * df
    if abs(f_top - fc) > 0.03 * fc:
        return dict(resolved=False, why="the highest point in the window is at %.1f Hz, %.0f%% from the reference %.1f Hz: no distinct peak here (broad, overlapping response)"
                    % (f_top, 100 * abs(f_top - fc) / fc, fc))
    half = top / 2.0
    i = i0
    while i > 0 and P[i] >= half:
        i -= 1
    j = i0
    while j < len(P) - 1 and P[j] >= half:
        j += 1
    if i == 0 or j == len(P) - 1:
        return dict(resolved=False, why="the peak is wider than the +-%.1f Hz analysis window" % hw)
    fl = (k0 + i + (half - P[i]) / (P[i + 1] - P[i])) * df      # linear interpolation of the half-power crossings
    fr = (k0 + j - 1 + (P[j - 1] - half) / (P[j - 1] - P[j])) * df
    sl = slice(i + 1, j)
    f = (k0 + np.arange(len(P))) * df
    f0 = float(np.sum(f[sl] * P[sl]) / np.sum(P[sl]))
    fwhm = float(fr - fl)
    return dict(resolved=True, f=f0, fwhm=fwhm, t60eq=2.199 / fwhm, gain_db=float(10 * np.log10(top)),
                f_top=float((k0 + i0) * df), bins_in_fwhm=float(fwhm / df))


def half_widths(freqs):
    hw = []
    for i, f in enumerate(freqs):
        nb = [abs(f - g) for j, g in enumerate(freqs) if j != i]
        d = 0.35 * min(nb) if nb else 1e9
        hw.append(float(max(1.0, min(0.25 * f, d, 0.4 * FS / 2 - f))))
    return hw


def resonances(wavs, rep):
    out = OrderedDict()
    ref = {}
    plan = []
    for pid in ("P3c", "P4"):
        for jack in (1, 2):
            if (pid, jack) in wavs:
                y = wavs[(pid, jack)].x[:, 0]
                modes = find_modes(y[int(NOISE_A[0] * FS):int(NOISE_A[1] * FS)])
                if pid == "P3c":
                    ref[jack] = modes
                plan.append((pid, jack, modes))
    for pid in ("P3a", "P3b"):
        for jack in (1, 2):
            if (pid, jack) in wavs and jack in ref:
                plan.append((pid, jack, ref[jack]))
    for pid, jack, modes in sorted(plan, key=lambda p: (p[1], p[0])):
        name = "%s %s" % (pid, C.JACK_NAME[jack])
        if len(modes) != 3:
            rep.add("Resonances", "warning", "%s: found %d strong spectral peaks, expected 3 per jack with all six sliders up" % (name, len(modes)))
        Hfun, df = response_spectrum(wavs[(pid, jack)].x[:, 0])
        rows = []
        for (fc, lvl), hw in zip(modes, half_widths([m[0] for m in modes])):
            m = measure_peak(Hfun, df, fc, hw)
            row = dict(f_candidate_hz=r1(fc), window_hw_hz=r1(hw))
            if not m["resolved"]:
                row["resolved"] = False
                rep.add("Resonances", "warning", "%s: ~%.1f Hz not measured: %s" % (name, fc, m["why"]))
            else:
                row.update(resolved=True, f_hz=round(m["f"], 4), fwhm_hz=round(m["fwhm"], 4), t60eq_s=round(m["t60eq"], 4), gain_db=r1(m["gain_db"]),
                           bins_in_fwhm=r1(m["bins_in_fwhm"]))
                nn, cc = C.note_name(m["f"])
                rep.add("Resonances", "notice", "%s: %.3f Hz (nearest 12-TET %s %+.1f c), FWHM %.3f Hz, T60-equivalent %.2f s, peak gain %.1f dB (includes the Scarlett)"
                        % (name, m["f"], nn, cc, m["fwhm"], m["t60eq"], m["gain_db"]))
            rows.append(row)
        out["%s_%s" % (pid, C.JACK_NAME[jack])] = rows
    rep.data["resonances"] = out
    return ref


# ---------------------------------------------------------------------------------------------------------------------
# Level linearity from the stepped noise

def power_sigma_db(x):
    """1-sigma statistical uncertainty (dB) of a mean-square level measured from Gaussian-like signal x.

    Narrow-band noise (a high-Q resonator driven by noise) has few independent samples, so its measured power wanders.
    Uses var(mean(x^2)) / mean(x^2)^2 = (2/N) * sum_k (1 - |k|/N) rho_k^2, with rho from the signal's own autocorrelation (lags below N/4).
    """
    x = np.asarray(x, dtype=np.float64)
    x = x - x.mean()
    n = len(x)
    nf = 1 << int(np.ceil(np.log2(2 * n)))
    X = np.fft.rfft(x, nf)
    r = np.fft.irfft(np.abs(X) ** 2, nf)[:n // 4]
    k = np.arange(len(r))
    rho = (r / (n - k)) / (r[0] / n)
    s = 1.0 + 2.0 * np.sum((1 - k[1:] / n) * rho[1:] ** 2)
    return float(4.343 * np.sqrt(2.0 * s / n))


def linearity(wavs, rep):
    res = OrderedDict()
    steps = C.MAIN["steps_db"]
    for pid in ("P3a", "P3b", "P3c", "P4"):
        for jack in (1, 2):
            if (pid, jack) not in wavs:
                continue
            y = wavs[(pid, jack)].x[:, 0]
            floor_db = rms_db(y[int(0.2 * FS):int(1.8 * FS)])
            pf = 10 ** (floor_db / 10)
            lin, net, sig = [], [], []
            raw = []
            for i, lvl in enumerate(steps):
                t0 = C.MAIN["step_t0"] + C.MAIN["step_period"] * i
                seg = y[int((t0 + 2.0) * FS):int((t0 + C.MAIN["step_noise"] - 0.1) * FS)]
                if len(seg) < FS:
                    continue
                p = 10 ** (rms_db(seg) / 10)
                raw.append(10 * np.log10(p))
                if p - pf > 10 * pf:   # at least 10 dB above the recorded floor
                    lin.append(lvl)
                    net.append(10 * np.log10(p - pf))
                    sig.append(power_sigma_db(seg))
            key = "%s_%s" % (pid, C.JACK_NAME[jack])
            if len(lin) < 3:
                res[key] = dict(usable_steps=len(lin), floor_dbfs=r1(floor_db), raw_step_rms_dbfs=[r1(v) for v in raw])
                rep.add("Level linearity (M1-n)", "warning", "%s: only %d steps are 10 dB above the floor (%.1f dBFS)" % (key, len(lin), floor_db))
                continue
            lin, net, sig = np.array(lin, dtype=float), np.array(net), np.array(sig)
            slope = float(np.polyfit(lin, net, 1)[0])
            d = net - lin
            dev = d - np.mean(d)   # deviation from the best-fitting straight 1:1 line
            zmax = float(np.max(np.abs(dev) / sig))
            res[key] = dict(usable_steps=len(lin), input_steps_dbfs=[int(v) for v in lin], slope_db_per_db=r1(slope),
                            max_dev_db=r1(np.max(np.abs(dev))), sigma_median_db=r1(np.median(sig)), max_dev_in_sigmas=r1(zmax),
                            gain_db=r1(np.mean(d)), floor_dbfs=r1(floor_db), raw_step_rms_dbfs=[r1(v) for v in raw])
            rep.add("Level linearity (M1-n)", "notice", "%s: %d steps (%d to %d dBFS in), slope %.3f, largest deviation from a 1:1 line %.2f dB "
                    "(statistical 1-sigma about %.2f dB, so %.1f sigma), mean gain %.1f dB (rms out minus rms in, includes the Scarlett)"
                    % (key, len(lin), lin[0], lin[-1], slope, np.max(np.abs(dev)), np.median(sig), zmax, np.mean(d)))
    rep.data["linearity"] = res


# ---------------------------------------------------------------------------------------------------------------------

def sweep_crosscheck(wavs, ref, rep):
    for pid, jack in (("P1", 1), ("P2", 2)):
        if (pid, jack) not in wavs or jack not in ref:
            continue
        f, _ = sweep_peak(wavs[(pid, jack)].x[:, 0])
        near = min(ref[jack], key=lambda m: abs(np.log(m[0] / f)))
        r = C.cents(f, near[0])
        ok = abs(r) < 100
        rep.add("Sweep cross-check", "notice" if ok else "warning",
                "%s: coarse sweep peak %.0f Hz; nearest resonance frequency on the same jack %.1f Hz (%+.0f cents). %s"
                % (pid, f, near[0], r, "Consistent with the recordings starting at the project start." if ok else "Larger than the 50 ms coarse resolution allows; check alignment."))


def analyse(capdir):
    rep = Report()
    files, wavs = load_capture(capdir, rep)
    inventory(files, wavs, rep)
    alignment(wavs, rep)
    mapping(wavs, rep)
    noise_source(wavs, rep)
    ref = resonances(wavs, rep)
    linearity(wavs, rep)
    sweep_crosscheck(wavs, ref, rep)
    return rep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("capdir")
    ap.add_argument("--json")
    a = ap.parse_args()
    rep = analyse(a.capdir)
    rep.flush()
    if a.json:
        with open(a.json, "w") as f:
            json.dump(rep.data, f, indent=1)
    return 1 if rep.worst() >= 2 else 0


if __name__ == "__main__":
    sys.exit(main())
