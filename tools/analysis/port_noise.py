#!/usr/bin/env python3
"""Milestone 3, fourth part: tests of the Q max width gap (docs/open-questions.md M3-k).

Usage: port_noise.py HWDIR --render PATH/smr_render [--render-fma PATH/smr_render_fma] [--seeds N] [--task floor,noise,segments,fma] [--json OUT.json]

At Q count 4095 the port's widths are 0.58 to 0.62 Hz and four of the six hardware widths are narrower than any width the port gives
at any Q count (port_fit.py, task qfit). This script tests, with the same measurement code as the hardware (analyse_capture
response_spectrum and measure_peak, through port_fit.measure), whether the gap can come from

  floor     What the hardware's own noise floor looks like. The floor (the recording's silent start and two later silent stretches) is
            measured: its rms, how much of its power lies within +-3 Hz of the three resonances (white noise added after the filters would
            put almost none there), and how far it is below the output when the recording's own -20 dBFS test noise drives the filters.
  noise     Noise in the hardware recording. Noise is added to the port at the hardware floor level and the widths measured again.
            Model A: white noise added to the port OUTPUT. Model B: white noise added to the port INPUT, so the resonators shape it;
            its level is set so the port's steady-state output noise has the hardware floor rms. The floor measurement above decides
            which model applies. Model B is swept over levels from -15 dB to 0 dB relative to the floor level with several noise seeds,
            and for each hardware width the script reports where it falls among the port-plus-noise widths.
  segments  Repeatability of the width measurement: the same peaks measured on two disjoint stretches of the recording (2 to 40 s: the
            20 s noise and its ring-out; 55 to 105 s: the level steps), for the hardware and for the noise-free port.
  fma       (only with --render-fma) Float rounding. The firmware is built -Ofast for a Cortex-M4F (third_party/SMR/Makefile), which has
            fused multiply-add; the port is built with it off. smr_render_fma is the same port source built with -ffp-contract=fast -mfma
            (CMake option SMR_FP_CONTRACT=fast). It shows how SENSITIVE the widths are to rounding; it does not reproduce the
            firmware's exact instruction sequence, which is not known.

The hardware floor is scaled into the port's units by the common gain offset between port and hardware at the peaks of the pass (the median
over the three peaks of each jack), the same offset compare_port.py removes. Nothing in the port is changed to fit the hardware.
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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyse_capture as A   # noqa: E402
import port_fit as F          # noqa: E402
import port_passes as P       # noqa: E402
import smrcap as C            # noqa: E402

FS = C.FS
JACKS = F.JACKS
JACK_NO = {"odds": 1, "evens": 2}
FLOOR_WINDOWS = ((0.2, 1.8), (26.0, 31.0), (50.5, 54.5))   # silent stretches: the start, after the 20 s noise rang out, before the level steps
DRIVEN = (6.0, 22.0)                                        # the recording's own -20 dBFS test noise
PASS_Q = (("P3c", 4095), ("P3b", 1876))                     # Q max (M3-k) and the Q count that fitted P3b (M3-h) as a control
LEVELS_DB = (-15.0, -10.0, -6.0, -3.0, 0.0)                 # model B levels relative to the hardware floor level
SEG = OrderedDict([("S1", (2.0, 40.0)), ("S2", (55.0, 105.0))])   # S1: the 20 s noise and its ring-out; S2: the level steps
CAL_SEC, CAL_USE = 60.0, 30.0            # calibration render for model B: 60 s of noise, the last 30 s measured
CAL_STD = 0.1                            # std of the calibration noise in the file (before the renderer's -30 dB)
X_RMS = 0.1                              # rms of the recording's test noise (-20 dBFS), the same units as CAL_STD


class Runner:
    def __init__(self, render, work, gain_db=-30.0):
        self.render, self.work, self.gain_db = render, work, gain_db
        self.n = 0
        self.sat = 0

    def run(self, inwav, qpot):
        self.n += 1
        out = os.path.join(self.work, "n%03d" % self.n)
        tmp = os.path.join(out, "_t")
        os.makedirs(tmp, exist_ok=True)
        stdout = P.render_pass(self.render, inwav, out, "X", "X", qpot, 0, P.BASE_CHANS, P.ODDS_TRACKOFFSET, self.gain_db, tmp, F.CORE_RATE)
        m = re.search(r"audio saturation events (\d+), search-loop guard trips (\d+)", stdout)
        if m:
            self.sat += int(m.group(1)) + int(m.group(2))
        res = {"odds": C.read_wav(os.path.join(out, "X_in1_oddsOUT.wav")).x[:, 0].astype(np.float64),
               "evens": C.read_wav(os.path.join(out, "X_in2_evensOUT.wav")).x[:, 0].astype(np.float64)}
        shutil.rmtree(out)
        return res


def rms(y):
    y = np.asarray(y, dtype=np.float64)
    return float(np.sqrt(np.mean(y * y)))


def db(v):
    return 20.0 * np.log10(v)


def rows_to_arrays(rows):
    """[dict or None] -> (f, fwhm, gain) arrays with nan for an unresolved peak."""
    f = np.array([r["f"] if r else np.nan for r in rows])
    w = np.array([r["fwhm"] if r else np.nan for r in rows])
    g = np.array([r["gain"] if r else np.nan for r in rows])
    return f, w, g


_SEG_X = {}


def seg_response(y, t0, t1):
    """Same as analyse_capture.response_spectrum, but only the stretch t0..t1 of input and output is used (the rest is set to zero)."""
    x = C.main_input()
    y = np.asarray(y, dtype=np.float64)
    N = 1 << int(np.ceil(np.log2(max(len(y), int(C.MAIN["total"] * FS)) + 1)))
    a, b = int(t0 * FS), min(int(t1 * FS), len(x))
    key = (N, a, b)
    if key not in _SEG_X:
        _SEG_X.clear()
        xs = np.zeros(len(x))
        xs[a:b] = x[a:b]
        X = np.fft.rfft(xs, N)
        _SEG_X[key] = (X, 1e-3 * float(np.mean(np.abs(X[1000:200000]) ** 2)))
    X, eps = _SEG_X[key]
    ys = np.zeros(len(y))
    b2 = min(int(t1 * FS), len(y))
    ys[a:b2] = y[a:b2]
    Y = np.fft.rfft(ys, N)
    df = FS / N

    def H(f_lo, f_hi):
        k0, k1 = max(1, int(f_lo / df)), min(len(X) - 1, int(f_hi / df) + 1)
        box = np.ones(9) / 9.0
        num = np.convolve(Y[k0:k1] * np.conj(X[k0:k1]), box, mode="same")
        den = np.convolve(np.abs(X[k0:k1]) ** 2, box, mode="same")
        return k0, num / (den + eps)
    return H, df


def measure_seg(y, cands_hz, t0, t1, scale):
    Hfun, df = seg_response(y, t0, t1)
    out = []
    for fc, hw in zip(cands_hz, A.half_widths(list(cands_hz))):
        m = A.measure_peak(Hfun, df, fc / scale, hw / scale)
        out.append(dict(f=m["f"] * scale, fwhm=m["fwhm"] * scale, gain=m["gain_db"]) if m["resolved"] else None)
    return out


def pct(a, b):
    return 100.0 * (np.asarray(a) / np.asarray(b) - 1.0)


def fmt(v, spec="%+.1f"):
    return " ".join((spec % x) if np.isfinite(x) else "n/a" for x in np.atleast_1d(v))


def floor_stats(y, peaks):
    """rms (dBFS) of one silent stretch pooled over FLOOR_WINDOWS (power mean), fraction of power within +-3 Hz of the peaks, per-window rms."""
    pw, near_pw, per = [], [], []
    for t0, t1 in FLOOR_WINDOWS:
        seg = np.asarray(y[int(t0 * FS):int(t1 * FS)], dtype=np.float64)
        n = len(seg)
        S = np.abs(np.fft.rfft((seg - seg.mean()) * np.hanning(n))) ** 2
        f = np.fft.rfftfreq(n, 1.0 / FS)
        near = np.zeros(len(f), bool)
        for p in peaks:
            near |= np.abs(f - p) < 3.0
        pw.append(np.mean(seg ** 2))
        near_pw.append(S[near].sum() / S.sum())
        per.append(db(np.sqrt(pw[-1])))
    return float(np.sqrt(np.mean(pw))), float(np.mean(near_pw)), per


# ---------------------------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("hwdir")
    ap.add_argument("--render", required=True)
    ap.add_argument("--render-fma", default=None, help="smr_render built with SMR_FP_CONTRACT=fast (task fma)")
    ap.add_argument("--seeds", type=int, default=8)
    ap.add_argument("--task", default="floor,noise,segments,fma")
    ap.add_argument("--json")
    a = ap.parse_args()
    tasks = a.task.split(",")
    if "fma" in tasks and not a.render_fma:
        tasks.remove("fma")

    _, hw_wavs = A.load_capture(a.hwdir, A.Report())
    ref = F.hw_reference(hw_wavs)
    for pid, _q in PASS_Q:
        for j in JACKS:
            if "%s_%s" % (pid, j) not in ref:
                C.emit("error", "port_noise: hardware reference incomplete", ["fewer than three resolved peaks for %s_%s" % (pid, j)])
                return 1

    work = tempfile.mkdtemp(prefix="portnoise_")
    R = Runner(a.render, work)
    result = OrderedDict()
    try:
        inwav = os.path.join(work, "in_mono.wav")
        X = C.main_input()
        P.write_wav_float_mono(inwav, X)

        # --- the hardware floor -------------------------------------------------------------------------------------
        floor_hw, hwr = {}, {}
        lines = ["Hardware silent stretches %s s pooled (power mean). 'near' = share of the floor's power within +-3 Hz of the three resonances "
                 "(the three bands are 0.04 %% of the 0-48 kHz span, so white noise would give 0.04 %%). 'driven' = rms in %.0f-%.0f s, "
                 "when the recording's own -20 dBFS noise drives the filters." % (", ".join("%.1f-%.1f" % w for w in FLOOR_WINDOWS), DRIVEN[0], DRIVEN[1])]
        fl_res = OrderedDict()
        for pid, _q in PASS_Q:
            for j in JACKS:
                key = "%s_%s" % (pid, j)
                y_hw = hw_wavs[(pid, JACK_NO[j])].x[:, 0]
                peaks = [r["f_hz"] for r in ref[key]]
                r_pool, near, per = floor_stats(y_hw, peaks)
                floor_hw[key] = r_pool
                drv = db(rms(y_hw[int(DRIVEN[0] * FS):int(DRIVEN[1] * FS)]))
                hwr[key] = dict(f=np.array(peaks), w=np.array([r["fwhm_hz"] for r in ref[key]]), g=np.array([r["gain_db"] for r in ref[key]]))
                fl_res[key] = dict(floor_dbfs=db(r_pool), per_window_dbfs=per, near_fraction=near, driven_dbfs=drv)
                lines.append("%s: floor %.1f dBFS rms (windows %s), %.1f %% of its power near the resonances; driven %.1f dBFS; driven minus floor %.1f dB"
                             % (key, db(r_pool), "/".join("%.1f" % v for v in per), 100 * near, drv, drv - db(r_pool)))
        if "floor" in tasks:
            C.emit("notice", "M3-k hardware noise floor", lines)
        result["floor"] = fl_res

        # --- noise-free port: the baseline for everything below ----------------------------------------------------
        base, gain_off, target = {}, {}, {}
        lines = []
        for pid, q in PASS_Q:
            res = R.run(inwav, q)
            base[pid] = res
            for j in JACKS:
                key = "%s_%s" % (pid, j)
                f, w, g = rows_to_arrays(F.measure(res[j], list(hwr[key]["f"])))
                gain_off[key] = float(np.nanmedian(g - hwr[key]["g"]))
                target[key] = floor_hw[key] * 10 ** (gain_off[key] / 20.0)
                hwr[key]["port"] = dict(f=f, w=w, g=g)
                lines.append("%s: Q count %d; hardware widths %s Hz; noise-free port %s Hz (%s %%); gain offset port minus hardware %+.2f dB; "
                             "hardware floor in port units %.1f dBFS"
                             % (key, q, fmt(hwr[key]["w"], "%.3f"), fmt(w, "%.3f"), fmt(pct(w, hwr[key]["w"])), gain_off[key], db(target[key])))
        C.emit("notice", "M3-k baseline: noise-free port against the hardware (core rate %.4f Hz)" % F.RATE, lines)
        result["baseline"] = {k: dict(hw_w=v["w"].tolist(), port_w=v["port"]["w"].tolist(), port_f=v["port"]["f"].tolist(), gain_off_db=gain_off[k],
                                      target_dbfs=db(target[k])) for k, v in hwr.items()}

        # --- 1. noise -----------------------------------------------------------------------------------------------
        if "noise" in tasks:
            noise_res = OrderedDict()
            for pid, q in PASS_Q:
                # model B calibration: the port's steady-state output rms for white input noise of std CAL_STD
                z0 = np.random.default_rng(777).standard_normal(int(CAL_SEC * FS)) * CAL_STD
                calwav = os.path.join(work, "cal.wav")
                P.write_wav_float_mono(calwav, z0)
                cal = R.run(calwav, q)
                os.remove(calwav)
                r0 = {j: rms(cal[j][int((CAL_SEC - CAL_USE) * FS):int(CAL_SEC * FS)]) for j in JACKS}
                need = {j: CAL_STD * target["%s_%s" % (pid, j)] / r0[j] for j in JACKS}   # file std that gives the hardware floor level on that jack
                zstd = float(np.sqrt(need["odds"] * need["evens"]))
                lines = ["%s model B at the floor level: white noise in the file of rms %.4g, which is %.1f dB relative to the recording's test noise "
                         "(rms %.2g) at the input; port output noise against the hardware floor: odds %+.1f dB, evens %+.1f dB (0 = equal)."
                         % (pid, zstd, db(zstd / X_RMS), X_RMS, db(zstd / need["odds"]), db(zstd / need["evens"]))]
                # model A, once: white noise on the output at the floor level
                wa = []
                for sd in range(3):
                    rng = np.random.default_rng(1000 + sd)
                    wa.append({j: rows_to_arrays(F.measure(base[pid][j] + rng.standard_normal(len(base[pid][j])) * target["%s_%s" % (pid, j)], list(hwr["%s_%s" % (pid, j)]["f"])))[1] for j in JACKS})
                lines.append("%s model A (white noise on the OUTPUT at the floor level, 3 seeds): largest change of any width from noise-free %.2f %%"
                             % (pid, max(np.nanmax(np.abs(pct(s[j], hwr["%s_%s" % (pid, j)]["port"]["w"]))) for s in wa for j in JACKS)))
                C.emit("notice", "M3-k noise test, %s: calibration and model A" % pid, lines)

                # model B sweep: whole recording, S1 and S2
                lvl_res = OrderedDict()
                for ldb in LEVELS_DB:
                    mult = 10 ** (ldb / 20.0)
                    rows = {j: {"whole": [], "S1": [], "S2": []} for j in JACKS}
                    for sd in range(a.seeds):
                        rng = np.random.default_rng(5000 + 31 * sd + int(10 * (ldb + 20)))
                        nw = os.path.join(work, "noisy.wav")
                        P.write_wav_float_mono(nw, X + rng.standard_normal(len(X)) * zstd * mult)
                        ys = R.run(nw, q)
                        os.remove(nw)
                        for j in JACKS:
                            cands = list(hwr["%s_%s" % (pid, j)]["f"])
                            rows[j]["whole"].append(rows_to_arrays(F.measure(ys[j], cands))[1])
                            for nm, (t0, t1) in SEG.items():
                                rows[j][nm].append(rows_to_arrays(measure_seg(ys[j], cands, t0, t1, F.SCALE))[1])
                    lvl_res[ldb] = {j: {k: np.array(v) for k, v in rows[j].items()} for j in JACKS}
                noise_res[pid] = lvl_res

                # report
                lines = ["%s, model B, %d seeds per level. Width in Hz: hardware | noise-free port | port plus noise, mean and std over seeds. z = (hardware - mean) / std."
                         % (pid, a.seeds)]
                zbest = OrderedDict()
                for ldb in LEVELS_DB:
                    zs, parts = [], []
                    for j in JACKS:
                        key = "%s_%s" % (pid, j)
                        Wd = lvl_res[ldb][j]["whole"]
                        m, sdv = np.nanmean(Wd, axis=0), np.nanstd(Wd, axis=0, ddof=1)
                        z = (hwr[key]["w"] - m) / sdv
                        zs.extend(z.tolist())
                        parts.append("%s mean %s sd %s z %s" % (j, fmt(m, "%.3f"), fmt(sdv, "%.3f"), fmt(z, "%+.1f")))
                    zbest[ldb] = float(np.sqrt(np.mean(np.square(zs))))
                    lines.append("level %+.0f dB (input noise %.1f dB re test noise): %s | rms z over the 6 peaks %.2f" % (ldb, db(zstd * 10 ** (ldb / 20.0) / X_RMS), " | ".join(parts), zbest[ldb]))
                best = min(zbest, key=zbest.get)
                lines.append("level with the smallest rms z: %+.0f dB (rms z %.2f)." % (best, zbest[best]))
                C.emit("notice", "M3-k noise sweep, %s, widths of the whole recording" % pid, lines)

                # S2 against S1, hardware against port plus noise
                lines = ["%s: width ratio S2/S1 (S1 = 2-40 s, S2 = 55-105 s) per peak; hardware | noise-free port | port plus noise (mean over seeds)." % pid]
                for j in JACKS:
                    key = "%s_%s" % (pid, j)
                    y_hw = hw_wavs[(pid, JACK_NO[j])].x[:, 0]
                    cands = list(hwr[key]["f"])
                    s1 = rows_to_arrays(measure_seg(y_hw, cands, *SEG["S1"], 1.0))[1]
                    s2 = rows_to_arrays(measure_seg(y_hw, cands, *SEG["S2"], 1.0))[1]
                    p1 = rows_to_arrays(measure_seg(base[pid][j], cands, *SEG["S1"], F.SCALE))[1]
                    p2 = rows_to_arrays(measure_seg(base[pid][j], cands, *SEG["S2"], F.SCALE))[1]
                    parts = ["hardware %s | noise-free %s" % (fmt(s2 / s1, "%.2f"), fmt(p2 / p1, "%.2f"))]
                    for ldb in LEVELS_DB:
                        d = lvl_res[ldb][j]
                        parts.append("%+.0f dB: %s" % (ldb, fmt(np.nanmean(d["S2"] / d["S1"], axis=0), "%.2f")))
                    lines.append("%s: %s" % (j, " | ".join(parts)))
                C.emit("notice", "M3-k noise sweep, %s, S2 against S1" % pid, lines)
            result["noise"] = {pid: {str(l): {j: {k: v.tolist() for k, v in d[j].items()} for j in JACKS} for l, d in lv.items()} for pid, lv in noise_res.items()}

        # --- 2. segment repeatability -------------------------------------------------------------------------------
        if "segments" in tasks:
            seg_res = OrderedDict()
            lines = ["Width (Hz) of the same peaks on the whole recording and on two disjoint stretches (S1 2-40 s, S2 55-105 s); hardware and noise-free port."]
            for pid, q in PASS_Q:
                for j in JACKS:
                    key = "%s_%s" % (pid, j)
                    y_hw = hw_wavs[(pid, JACK_NO[j])].x[:, 0]
                    cands = list(hwr[key]["f"])
                    entry = OrderedDict()
                    for who, y, sc in (("hardware", y_hw, 1.0), ("port", base[pid][j], F.SCALE)):
                        w1 = rows_to_arrays(measure_seg(y, cands, *SEG["S1"], sc))[1]
                        w2 = rows_to_arrays(measure_seg(y, cands, *SEG["S2"], sc))[1]
                        wf = hwr[key]["w"] if who == "hardware" else hwr[key]["port"]["w"]
                        entry[who] = dict(whole=wf, S1=w1, S2=w2)
                        lines.append("%s %s: whole %s | S1 %s | S2 %s | S2 vs S1 %s %%" % (key, who, fmt(wf, "%.3f"), fmt(w1, "%.3f"), fmt(w2, "%.3f"), fmt(pct(w2, w1))))
                    seg_res[key] = {who: {k: v.tolist() for k, v in e.items()} for who, e in entry.items()}
            result["segments"] = seg_res
            C.emit("notice", "M3-k width measurement repeatability, two stretches of the recording", lines)

        # --- 3. rounding sensitivity --------------------------------------------------------------------------------
        if "fma" in tasks:
            R2 = Runner(a.render_fma, work)
            lines = ["Port built with -ffp-contract=fast -mfma (fused multiply-add allowed) against the normal port, same renders as the baseline. "
                     "The firmware's own instruction sequence is not known; this only shows how sensitive the widths are to rounding."]
            fma_res = OrderedDict()
            for pid, q in PASS_Q:
                res = R2.run(inwav, q)
                for j in JACKS:
                    key = "%s_%s" % (pid, j)
                    f, w, g = rows_to_arrays(F.measure(res[j], list(hwr[key]["f"])))
                    rel = float(np.sqrt(np.mean((res[j] - base[pid][j]) ** 2)) / rms(base[pid][j]))
                    fma_res[key] = dict(f=f.tolist(), w=w.tolist(), g=g.tolist(), rel_rms_diff=rel)
                    lines.append("%s: fused widths %s Hz; vs hardware %s %%; vs the unfused port %s %%; frequency change %s cent; output differs from the unfused one by %.1f dB (rms, relative)"
                                 % (key, fmt(w, "%.3f"), fmt(pct(w, hwr[key]["w"])), fmt(pct(w, hwr[key]["port"]["w"]), "%+.3f"),
                                    fmt(C.cents(f, hwr[key]["port"]["f"]), "%+.4f"), db(rel)))
            lines.append("saturation events plus loop-guard trips in the fused renders: %d" % R2.sat)
            result["fma"] = fma_res
            C.emit("notice", "M3-k rounding sensitivity (fused multiply-add)", lines)

        C.emit("warning" if R.sat else "notice", "port_noise renders", ["%d renders; audio saturation events plus search-loop guard trips over all of them: %d" % (R.n, R.sat)])
    finally:
        shutil.rmtree(work, ignore_errors=True)
    if a.json:
        with open(a.json, "w") as f:
            json.dump(result, f, indent=1, default=float)
    return 0


if __name__ == "__main__":
    sys.exit(main())
