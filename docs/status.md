# Status

## Milestone 0: Foundations. DONE (confirmed 2026-10-25)

Confirmed by CI run 40237397289 (commit 856d513, green):
- third_party/SMR submodule at 79bc953bb4b9cf47614d317b4ba9fc9f615c43ea (checked in CI)
- AU and VST3 universal (x86_64 + arm64), minimum macOS 12.0 on every slice
- auval on the CI runner: AU VALIDATION SUCCEEDED (arm64 runner)
- tests/core_test.cpp (bit-exact pass-through) passed in CI

Confirmed by Bryn's own Logic test (docs/testing-milestone-0.md, checks 1-6):
- M4 mac mini, macOS Tahoe, Logic 11.2: all passed
- Intel Mac, macOS 12.7.8, Logic 10.7.8: all passed

Not tested: VST3 in any host (Logic loads AU only).

Choices made (see docs/open-questions.md): JUCE 8.0.6, AU type effect (aufx), codes Ptdt/Smru,
runner macos-15 (macos-14 retires 2 Nov 2026).

## Milestone 1: Source audit. DONE (approved 2026-10-25)

Bryn approved the keep/stub/drop list (architecture.md section 6) and decided to follow the source
for every open item M1-a to M1-t. M1-m (output labelling): source mapping for now, Bryn chooses
left/right later. Still undecided and not needed yet: Q1 to Q5 (licensing), v5 feature scope,
Freq as MIDI or parameter.

## Milestone 2: Headless DSP core. CI GREEN 2026-10-25 (commit after 7dc7b37); waiting for Bryn's renderer test

Confirmed by CI annotations (run 40246929819, jobs oracle-linux and core-macos):
- V1a: the tables the firmware really uses equal the port's tables (46622 words).
- V1: port and unmodified firmware files give identical traces (outputs, qval, notes, scales, morph, nudge, shift, levels, Env Out), 142 words per block.
  Scenarios 0, 1, 2, 3, 5, 6: 30000 blocks each. Scenario 7: 9143 blocks. Scenario 4: only 588 blocks.
  Scenarios 4 and 7 reach output saturation after that point, where the x86 oracle and the port differ by design (docs/port-notes.md P-3). So mode/type switching is compared over a short span only.
- V1 negative control: one changed input sample is detected.
- Core tests: 61 checks pass on Linux x86-64, macOS arm64 and macOS x86-64 (Rosetta). V2, V3, V5 (default resonators at 65.40 to 130.80 Hz), scheduler, adapter, resampler.
- V4: identical fixed-render hash 7f50d4657b6d1e1c on arm64 and x86-64.
- V6: smr_render end to end at 44.1, 48 and 96 kHz, mono and stereo, controls and script render; an unsupported rate is refused. smr_render is universal, minos 12.0 on both slices.
- Artifact Unofficial-SMR-renderer-macOS uploaded.

Not confirmed: anything on real hardware. Output channel mapping (M1-m) and level scale (M1-n) are placeholders. The 96 kHz path is bit-exact against the direct core; the resampler paths are not compared with hardware.

Found on the way: a firmware out-of-range read, see open-questions.md P-5. Details of every difference from the source: docs/port-notes.md.

Bryn's renderer test, 2026-10-25:
- Intel MacBook Pro, macOS 12.7.8: ran cleanly on a 326 s, 48 kHz file. Zero underruns, zero loop-guard trips.
  Defaults (Two-Pass MAXQ, bank 0, Q pot 2048): peak 1.003, 6,286,732 saturation events. Sounds like a resonant low-frequency boost, "like the real SMR on a fairly extreme setting" (by ear, no side-by-side).
  With `--pot 0=1000` (lower Q): peak 1.001, 505,451 saturation events. Bryn's impression is that the clipping comes from the resonance. Not confirmed: the input file's level was not recorded and the level scale (M1-n) is a placeholder, so a hot level scale cannot be ruled out until a hardware capture.
  The quieter-sliders render was not run (command typo).
- M4 mac mini (Tahoe): NOT tested. Bryn chose to proceed to Milestone 3 without it. The gap stays open.
- Real SMR comparison: not done (hardware not to hand). Fidelity against the hardware is unconfirmed.

Note: CI logs cannot be downloaded in Claude's session (HTTP 403). The workflow writes results as annotations.

## Milestone 3: Hardware reference. IN PROGRESS: capture-1 recorded, measurement tool CI green, comparison with the port NOT started

Bryn first deferred the capture (2026-10-25), then recorded it on 2026-10-26 (release capture-1). The capture is needed before Milestone 5 (plugin DSP match) can be confirmed. Until the comparison is done, fidelity against the real SMR is unconfirmed.

Agreed so far, for when the capture is done:
- Recording happens on one Mac next to the SMR, with Bryn's Focusrite Scarlett 2i2 (3rd Gen). Line input max +22 dBu at minimum gain, line output max +15.5 dBu at 0 dBFS, up to 192 kHz (Focusrite product page; DC coupling not stated, so treat as AC coupled: no Env Out CV capture).
- Cables: three 3.5 mm to 1/4" mono (Scarlett out to SMR in, SMR odds out and evens out to Scarlett inputs 1 and 2).
- Recordings go to a GitHub release asset, not git; CI analyses them and writes annotations.
- Starting tolerances (Claude's proposal, Bryn agreed to start there): centre frequency 1 cent after the M2-b offset, gain 0.5 dB, decay time 10%. To be revised after real data.
- Tier A (audio only): noise source with nothing patched in, one slider at a time (output mapping M1-m), stepped sine levels against the clip LEDs (M1-n), Q at three knob positions, a high-frequency ring for 96,000 versus about 95,982 Hz (M2-b).
- Tier B (needs a CV source Bryn has not identified): Env Out (DC-coupled input needed), Scale CV step, Q CV, locked-channel Freq jack, Freq jack step response.

Placeholders that stay until the capture: output mapping (M1-m), level scale (M1-n), core rate constant (M2-b), main-loop rate (M2-c). Each is a single constant or mapping in the code.

### Capture 1: what was recorded and what the tool measured (2026-10-26)

Recorded by Bryn on the M4 mac mini, Scarlett 2i2 3rd Gen, cable in the LEFT output (the first attempt used RIGHT and was redone). Release `capture-1`, asset `SMRCapture.zip`, 233,152,665 bytes, sha256 `996aab32...ede291`. 14 WAV files (P0 to P4, both jacks), 96 kHz, 24-bit, mono, none clipped, none silent. P5 (loopback) was NOT recorded (it was marked optional). The SMR clip LED never lit (Bryn). Panel photos not seen by Claude. Bank and scale were "left as loaded" and are not yet identified in the source tables.

Confirmed by CI (commit 9e18c5d, workflow `analysis.yml`):
- Self-test of `tools/analysis` on fake recordings with known answers: 37 of 37 checks passed (run 40451981714). It validates the tool only; it says nothing about the SMR.
- Analysis of the real asset: run 40452093561, job analyse-capture, results as annotations (JSON kept as artifact `capture-analysis-result`). Measured values below are from that run.

| Item | Measured |
|---|---|
| Output mapping (M1-m) | Slider 1 leaves by the ODDS jack (jack levels -26.4 vs -92.4 dBFS rms), slider 2 by the EVENS jack (-92.8 vs -28.8). Assumes cabling and Logic inputs as the capture sheet says (Bryn confirmed both). Differs from manual and source: see M3-a. |
| Resonances, Q max (P3c) | Odds jack 218.674, 437.413, 874.966 Hz. Evens jack 408.682, 523.167, 1046.512 Hz. Full width at half power 0.46 to 0.62 Hz. |
| Resonances, rotate 10 (P4, Q max) | Odds jack 27.332, 7061.334, 14539.826 Hz. Evens jack 32.697, 13515.131, 17717.008 Hz. |
| Resonances, Q mid (P3b) | Same frequencies as P3c, full width 1.13 to 1.31 Hz. P3b and P3c frequencies agree within 0.14 cent (informal check). |
| Q min (P3a) | No distinct per-channel peaks (broad overlapping response); not measured per channel. |
| Noise source, nothing patched (P0) | rms -36.5 dBFS (odds) and -36.3 dBFS (evens), correlation between the jacks 0.809. |
| Level steps | Only the top 2 to 4 steps are 10 dB above the floor; statistical uncertainty of each step 0.08 to 1.8 dB (1 sigma). |

The hardware is not at the source default scale (65.40 to 130.80 Hz, M1-r). Not yet done: identify the bank and scale, render the port at the same settings, compare. M1-n (absolute level) stays open without P5. M2-b (96,000 versus 95,982 Hz) and M2-c cannot be judged until the port is rendered at the hardware's scale. Details and decisions needed: docs/open-questions.md M3-a to M3-f. Method and assumptions: tools/analysis/README.md.

## Milestone 4: Parameter layer and audio wiring (code written, NOT yet confirmed)

Written: 41 parameters (`src/plugin/ParamMap.*`), saved state (`StateCodec.*`, `Engine` snapshot in `src/core/Snapshot.cpp`), core wired into the plugin through `BlockAdapter` (`PluginProcessor.cpp`), generic editor with status line. Decisions and findings: docs/open-questions.md M4-a to M4-l.

Local result (sandbox GCC, core only): smr_param_tests 93 checks pass, smr_core_tests 61 checks pass, fixed-render hash unchanged (7f50d4657b6d1e1c). JUCE files were syntax-checked only in the sandbox.
Not confirmed until CI is green and Bryn has run docs/testing-milestone-4.md on both Macs: plugin build, parameter list, automation, saved state, audio, CPU. Fidelity against the hardware: still unconfirmed (M3 deferred).

### Milestone 4 test result (Bryn, 2026-10-26, both Macs, docs/testing-milestone-4.md)
- A loads, B all 41 parameters visible, D automation, E triggers, F locks and bank, G save and reopen, H mono track, J buffer-size change: all as expected, no problems reported.
- C: audible effect, bands audible, no distortion, no crackle, CPU 5% or less (rough reading, one instance), sound rings out for less than a few seconds (decay depends on Q; the 10 s figure is only the tail length reported to the host).
- I: status line latency 34 samples, both Macs at 48 kHz (Bryn, 2026-10-26).
- Not recorded: run number installed, underruns count, results broken down per Mac. Treated as the same on both.
- Not confirmed: fidelity to the real SMR (Milestone 3 deferred). "Sounds very much like the real SMR" is Bryn's ear on the Intel Mac, no side-by-side.
Milestone 4: PASSED on both Macs.
