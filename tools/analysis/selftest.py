#!/usr/bin/env python3
"""Self-test of the analysis tool on FAKE recordings with known answers. Run this in CI before any real data is analysed.

A pass here means only that the tool recovers known values from fake data and flags known faults. It says nothing about the
SMR. The tolerances below are for validating the tool and are tighter than the hardware tolerances (1 cent, 0.5 dB, 10%).
Usage: selftest.py [WORKDIR]
"""
import os
import shutil
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import analyse_capture as A   # noqa: E402
import fake_capture as F      # noqa: E402
import smrcap as C            # noqa: E402

FS = C.FS
RESULTS = []


def check(name, ok, detail=""):
    RESULTS.append((name, bool(ok), detail))


def texts(rep, topic):
    return [lv + ": " + ln for lv, ln in rep.topics.get(topic, [])]


def has(rep, topic, level, needle):
    return any(lv == level and needle in ln for lv, ln in rep.topics.get(topic, []))


# ---------------------------------------------------------------------------------------------------------------------
def test_timeline():
    g = F.load_gen()
    sw = g.sweep()
    check("timeline: sweep length 38 s", len(sw) == int(C.SWEEP["total"] * FS))
    s0, s1 = int(C.SWEEP["start"] * FS), int((C.SWEEP["start"] + C.SWEEP["dur"]) * FS)
    check("timeline: sweep silent before 3 s and after 33 s", np.max(np.abs(sw[:s0])) == 0 and np.max(np.abs(sw[s1:])) == 0)
    check("timeline: sweep active 3 to 33 s", np.max(np.abs(sw[s0 + FS:s1 - FS])) > 0.2)
    m = g.main_sig(np.random.default_rng(20261025))
    check("timeline: main length 105 s", len(m) == int(C.MAIN["total"] * FS))
    check("timeline: main silent before 2 s", np.max(np.abs(m[:2 * FS])) == 0)
    a0, a1 = [int(t * FS) for t in C.MAIN["noiseA"]]
    lv = 10 * np.log10(np.mean(m[a0:a1] ** 2))
    check("timeline: noise A is -20 dBFS rms from 2 to 22 s", abs(lv + 20) < 0.3, "%.2f" % lv)
    lo = 25 * FS   # the impulse section runs 25 to 55 s; the noise sections can also exceed 0.4 now and then
    imp = lo + np.where(np.abs(m[lo:55 * FS]) > 0.4)[0]
    exp = [int(round(t * FS)) for t in C.MAIN["impulses"]]
    check("timeline: five impulses at the expected samples, and nothing else above 0.4 in 25 to 55 s", [int(v) for v in imp] == exp, "%s vs %s" % ([int(v) for v in imp][:6], exp))
    ok = True
    for i, lvl in enumerate(C.MAIN["steps_db"]):
        t0 = C.MAIN["step_t0"] + C.MAIN["step_period"] * i
        seg = m[int((t0 + 0.1) * FS):int((t0 + 4.9) * FS)]
        ok &= abs(10 * np.log10(np.mean(seg ** 2)) - lvl) < 0.5
        ok &= np.max(np.abs(m[int((t0 + 5.0) * FS):int((t0 + 6.0) * FS)])) == 0
    check("timeline: eight level steps at the expected levels and times", ok)


def test_names():
    exp = {
        "P0-Noise_Qmin_in2_oddsOUTt.wav": ("P0", 1), "P0-Noise_Qmin_in2_evensOUT.wav": ("P0", 2),
        "P1_sweep_s1_Qmid_in1_oddsOUT.wav": ("P1", 1), "P1_sweep_s1_Qmid_in2_evenOUT.wav": ("P1", 2),
        "P2_sweep_s1_Qmid_in1_oddsOUT.wav": ("P2", 1), "P2_sweep_s1_Qmid_in2_evenOUT.wav": ("P2", 2),
        "P3a_main_Qmin_in1_oddsOUT.wav": ("P3a", 1), "P3a_main_Qmin_in2_evenOUT..wav": ("P3a", 2),
        "P3b_main_Qmid_in2_evenOUT..wav": ("P3b", 2), "P3c_main_Qmax_in1_oddsOUT.wav": ("P3c", 1),
        "P4_main_rot10_Qmax_in1_oddsOUT.wav": ("P4", 1), "P4_main_rot10_Qmax_in2_evenOUT.wav": ("P4", 2),
        "P5_loopback_in1.wav": ("P5", 1), "P1_sweep_s1_Qmid_in2_evensOUT.wav": ("P1", 2), "notes.txt": None,
    }
    bad = [n for n, e in exp.items() if C.classify(n) != e]
    check("names: the real capture-1 file names (typos included) classify correctly", not bad, ", ".join(bad))


def test_no_peak():
    """If the highest point in the window is not near the reference frequency, there is no distinct peak to measure."""
    rng = np.random.default_rng(4)
    y = F.through(C.main_input(), [(223.0, 0.08)], "max", rng, 1)
    Hf, df = A.response_spectrum(y)
    m = A.measure_peak(Hf, df, 245.0, 40.0)
    check("no peak: a window whose highest point is 9% from the reference is reported as not measured", (not m["resolved"]) and "no distinct peak" in m["why"], str(m))


def test_wide_peak():
    """A peak wider than the analysis window must be reported as not measured, not as a number."""
    rng = np.random.default_rng(3)
    old = F.T60["max"]
    F.T60["max"] = 0.01   # full width about 220 Hz
    try:
        y = F.through(C.main_input(), [(223.0, 0.08)], "max", rng, 1)
    finally:
        F.T60["max"] = old
    Hf, df = A.response_spectrum(y)
    m = A.measure_peak(Hf, df, 223.0, 8.9)
    check("wide peak: a 220 Hz wide peak in an 8.9 Hz window is reported as not measured", not m["resolved"], str(m))


# ---------------------------------------------------------------------------------------------------------------------
def near(a, b, tol):
    return abs(a - b) <= tol


def test_good(capdir):
    rep = A.analyse(capdir)
    d = rep.data
    check("good: no errors on a clean fake capture", rep.worst() < 2, str([t for t in rep.topics if any(lv == "error" for lv, _ in rep.topics[t])]))
    check("good: 14 files read, only the optional P5 missing", has(rep, "Inventory", "notice", "14 files read") and has(rep, "Inventory", "notice", "missing: P5"))
    check("good: no clipping or silence flagged", not any("clipped" in ln or "silent" in ln for _, ln in rep.topics.get("Inventory", [])))
    off = list(d["alignment_offsets_s"].values())
    check("alignment: all eight main files start at 0.00 s offset", len(off) == 8 and max(abs(v) for v in off) < 0.06, str(off))
    mp = d["mapping"]
    check("mapping: slider 1 on the odds jack, slider 2 on the evens jack", mp["P1"]["loud_jack"] == "odds" and mp["P2"]["loud_jack"] == "evens")
    check("mapping: the difference from the source is reported", mp.get("matches_source") is False and has(rep, "Output mapping (M1-m)", "warning", "DIFFERS"))
    worst_c = 0.0
    for key, truth in (("P3c_odds", F.ODDS), ("P4_odds", F.ODDS_ROT), ("P3c_evens", F.EVENS), ("P4_evens", F.EVENS_ROT)):
        rows = d["resonances"].get(key, [])
        fits = sorted(r["f_hz"] for r in rows if "f_hz" in r)
        tf = sorted(f for f, _ in truth)
        ok = len(fits) == 3 and all(abs(C.cents(a, b)) < 0.3 for a, b in zip(fits, tf))
        if len(fits) == 3:
            worst_c = max([worst_c] + [abs(C.cents(a, b)) for a, b in zip(fits, tf)])
        check("frequencies: %s recovered within 0.3 cent" % key, ok, "%s vs %s" % (fits, tf))
    n_ok, n_all, worst = 0, 0, 0.0
    for pid, q in (("P3a", "min"), ("P3b", "mid"), ("P3c", "max")):
        for jack in ("odds", "evens"):
            for row in d["resonances"].get("%s_%s" % (pid, jack), []):
                n_all += 1
                if row.get("resolved"):
                    e = abs(row["t60eq_s"] / F.T60[q] - 1)
                    worst = max(worst, e)
                    n_ok += e < 0.03
    check("decay: all 18 T60-equivalent values within 3% of the truth", n_all == 18 and n_ok == 18, "%d/%d within 3%%, worst %.1f%%" % (n_ok, n_all, 100 * worst))
    lin = d["linearity"]
    check("linearity: all 8 files give slope 1.00 +- 0.05 and deviations inside 4.5 sigma",
          len(lin) == 8 and all(abs(v["slope_db_per_db"] - 1) < 0.05 and v["max_dev_in_sigmas"] < 4.5 for v in lin.values()),
          str({k: (v.get("slope_db_per_db"), v.get("max_dev_in_sigmas")) for k, v in lin.items()}))
    ns = d["noise_source"]
    for jack, name in ((1, "odds"), (2, "evens")):
        w = C.read_wav(os.path.join(capdir, F.NAMES[("P0", jack)]))
        direct = A.rms_db(w.x[int(2 * FS):, 0])
        check("noise source: %s rms equals a direct calculation" % name, near(ns[name]["rms_dbfs"], direct, 0.01), "%s vs %.3f" % (ns[name]["rms_dbfs"], direct))
    check("sweep: coarse peaks agree with the measured resonance frequencies", has(rep, "Sweep cross-check", "notice", "P1") and not any(lv == "warning" for lv, _ in rep.topics.get("Sweep cross-check", [])))
    return rep


def test_faults(capdir):
    rep0 = A.Report()
    files, wavs = A.load_capture(capdir, rep0)

    def run(mut_files=None, mut_wavs=None, with_map=False):
        f2, w2 = dict(files), dict(wavs)
        for k, v in (mut_wavs or {}).items():
            w2[k] = v
        for k in (mut_files or {}):
            f2.pop(k, None)
            w2.pop(k, None)
        rep = A.Report()
        A.inventory(f2, w2, rep)
        if with_map:
            A.mapping(w2, rep)
        return rep, f2, w2

    key = ("P3a", 1)
    w = wavs[key]
    rep, _, _ = run(mut_wavs={key: w._replace(x=np.clip(w.x * 1000, -1, 1))})
    check("faults: a clipped file is an error", has(rep, "Inventory", "error", "clipped"))
    rep, _, _ = run(mut_wavs={key: w._replace(x=np.zeros_like(w.x))})
    check("faults: a silent file is an error", has(rep, "Inventory", "error", "silent"))
    rep, _, _ = run(mut_wavs={key: w._replace(x=w.x[:20 * FS])})
    check("faults: a short file is a warning", has(rep, "Inventory", "warning", "shorter than"))
    rep, _, _ = run(mut_wavs={key: w._replace(sr=44100)})
    check("faults: a 44.1 kHz file is an error", has(rep, "Inventory", "error", "sample rate"))
    rep, _, _ = run(mut_wavs={key: w._replace(bits=16)})
    check("faults: a 16-bit file is a warning", has(rep, "Inventory", "warning", "16-bit"))
    rep, _, _ = run(mut_files={("P4", 1): 1})
    check("faults: a missing pass is a warning", has(rep, "Inventory", "warning", "missing: P4 odds"))
    p1a, p1b = wavs[("P1", 1)], wavs[("P1", 2)]
    rep, _, w2 = run(mut_wavs={("P1", 1): p1b, ("P1", 2): p1a}, with_map=True)
    check("faults: swapped jacks in P1 are seen, mapping then differs from the P2 result",
          w2 is not None and rep.data["mapping"]["P1"]["loud_jack"] == "evens" and rep.data["mapping"].get("matches_source") is False)
    rep, _, _ = run(mut_wavs={("P1", 1): p1b, ("P2", 2): wavs[("P2", 1)], ("P1", 2): p1a, ("P2", 1): wavs[("P2", 2)]}, with_map=True)
    check("faults: with both sweeps swapped the mapping matches the source", rep.data["mapping"].get("matches_source") is True and not has(rep, "Output mapping (M1-m)", "warning", "DIFFERS"))
    # saturation: soft-compress the loud steps of P3b odds
    x = wavs[("P3b", 1)].x
    sat = np.tanh(x / 0.004) * 0.004
    w2 = dict(wavs)
    w2[("P3b", 1)] = wavs[("P3b", 1)]._replace(x=sat.astype(np.float32))
    rep = A.Report()
    A.linearity({k: v for k, v in w2.items() if k == ("P3b", 1)}, rep)
    z = rep.data["linearity"].get("P3b_odds", {}).get("max_dev_in_sigmas", 0)
    check("faults: a compressed (saturating) recording stands out from the statistical scatter", z > 6, "max deviation %.1f sigma" % z)
    # duplicate and unknown names
    tmp = tempfile.mkdtemp(prefix="dup_")
    try:
        for n in os.listdir(capdir):
            if n.startswith("P1_") or n.startswith("P2_"):
                os.symlink(os.path.join(capdir, n), os.path.join(tmp, n))
        os.symlink(os.path.join(capdir, F.NAMES[("P1", 1)]), os.path.join(tmp, "P1_again_in1_oddsOUT.wav"))
        os.symlink(os.path.join(capdir, F.NAMES[("P1", 1)]), os.path.join(tmp, "readme_copy.wav"))
        r = A.Report()
        A.load_capture(tmp, r)
        check("faults: two files for one pass and jack is an error", has(r, "Inventory", "error", "two files for P1 odds"))
        check("faults: an unrecognised file name is a warning", has(r, "Inventory", "warning", "unrecognised name: readme_copy.wav"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    work = sys.argv[1] if len(sys.argv) > 1 else tempfile.mkdtemp(prefix="smr_selftest_")
    capdir = os.path.join(work, "fake_capture")
    test_timeline()
    test_names()
    test_wide_peak()
    test_no_peak()
    F.build(capdir)
    test_good(capdir)
    test_faults(capdir)
    bad = [(n, d) for n, ok, d in RESULTS if not ok]
    good = [n for n, ok, _ in RESULTS if ok]
    C.emit("notice", "Analysis self-test: %d of %d checks passed" % (len(good), len(RESULTS)), good)
    if bad:
        C.emit("error", "Analysis self-test: %d checks FAILED" % len(bad), ["%s -- %s" % (n, d) for n, d in bad])
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
