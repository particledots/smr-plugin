# tools/analysis: Milestone 3 hardware-capture analysis

Measures the recordings made through the real SMR (docs/capture-sheet.md). It measures only: it does not yet compare with the port and it states no pass or fail against the agreed tolerances (1 cent, 0.5 dB, 10% decay). Nothing here is confirmed until a CI run shows it.

| File | What it is |
|---|---|
| `smrcap.py` | WAV reader (Logic's extra chunks tolerated), file-name matcher, signal timeline, annotations |
| `analyse_capture.py` | the analysis: `analyse_capture.py CAPDIR [--json OUT.json]` |
| `fake_capture.py` | fake recordings with known answers (plain two-pole resonators, not a model of the SMR) |
| `selftest.py` | runs the analysis on the fake recordings and checks it recovers the known values and flags known faults |

Needs numpy and scipy (`requirements.txt`). Run only in CI; never on Bryn's Macs.

## File names
The pass comes from the leading P-code. Any name containing `odds` is input 1 (odds jack); otherwise `even` means input 2; otherwise `in1` or `in2` decides (Bryn, 2026-10-26: the P0 odds file was typed `in2` by mistake). The P2 files are named `s1` but hold slider 2; the pass table in `smrcap.py` decides, not the name.

## What is measured, and the assumptions
- **Inventory**: sample rate, bit depth, length against what the signal needs, peak, clipped samples, silent files, missing passes.
- **Alignment**: when the output rises 10 dB above its floor, against the 2.00 s the noise starts. Judged on P3a and P3b only (high-Q passes build up slowly).
- **Output mapping (M1-m)**: which jack carries slider 1 (P1) and slider 2 (P2). Assumes the cabling and Logic inputs as the capture sheet says (confirmed by Bryn, 2026-10-26).
- **Noise source (P0)**: rms, peak and octave-band levels per jack, and how alike the two jacks are.
- **Resonances (P3a, P3b, P3c, P4)**: from H(f) = Y(f) / X(f), where X is the exact test signal regenerated from `tools/capture/gen_signals.py` and Y the whole recorded file. Per peak: centre frequency (power-weighted mean between the half-power points), full width at half power (FWHM), peak gain. "T60-equivalent" is 2.199 / FWHM, the ring time a single exponential with that width would have. The SMR in Two-Pass mode is not a single pole, so this is only a model-free number to compare hardware and port, measured the same way on both. Peak gain includes the Scarlett's own gain (P5 would separate the two).
  - Candidate peaks come from the Q max pass (P3c) and, for the rotated pass, P4. P3a and P3b are measured at the P3c frequencies. A window whose highest point is more than 3% from the reference is reported as "no distinct peak" (this is what the Q min pass gives on the real unit).
- **Level linearity (M1-n)**: output rms against input rms for the eight noise steps, with the recorded floor subtracted. Narrow-band noise has few independent samples, so each step has a statistical uncertainty (1 sigma, from the signal's own autocorrelation); the tool reports the deviation in dB and in sigmas. A 0.5 dB tolerance cannot be checked on a step whose uncertainty is larger than that.
- **Sweep cross-check**: coarse (about 20 cents) peak of the sweep against the resonance frequency on the same jack.

## Why the single-sample impulses are not analysed
On the real capture-1 recordings the five impulses in `smr_sig_main.wav` leave no ring above the SMR's own noise floor (about -60 dBFS at Q max, about -84 dBFS at Q min), so ring-down fits on them are meaningless. The steady noise carries the same information with far better signal-to-noise. The impulses stay in the signal file.

## Self-test tolerances
The self-test checks the tool, not the SMR. Its tolerances (0.3 cent, 3% on ring time, 4.5 sigma on linearity) are for validating the tool on fake data with a known answer and are not the hardware tolerances.
