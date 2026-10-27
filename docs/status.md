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

## Milestone 3: Hardware reference. IN PROGRESS: capture-1 recorded and compared in CI (runs 40454798347 and 40457983850). Frequencies match inside 1 cent at the hardware core rate with a whole-step odds Freq-jack offset; P3b widths match at a Q count below noon; Q max widths and the high-frequency gain tilt are measured but not matched in the port

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

The hardware is not at the source default scale (65.40 to 130.80 Hz, M1-r). M1-n (absolute level) stays open without P5. Details and decisions needed: docs/open-questions.md M3-a to M3-j. Method and assumptions: tools/analysis/README.md.

### Port against capture-1 (CI run 40454798347, commit 9757f39, job compare-port)

Port rendered by `tools/analysis/port_passes.py` (`smr_render`, GCC build, input -30 dB, no saturation, no loop-guard trips, no underruns), measured by the same `analyse_capture.py` code as the hardware, compared by `tools/analysis/compare_port.py`. Tolerances as agreed (1 cent, 10% width, 0.5 dB gain; one common gain offset per jack removed because the absolute level is unknown, M1-n). Mapping: right output to the odds file, left to the evens file (M3-a).

Identified state (from the source tables, see M3-f): all six channels bank 6 (channels 4 and 6 could also be bank 2), scales 2/10/2/2/2/2 (odds scale undetermined), notes 6 to 11 at P3.

| Variant | Frequency (18 peaks, 9 per jack) | Width inside 10% | Gain inside 0.5 dB |
|---|---|---|---|
| base (no offset) | 9 inside; odds jack -5.50 cent mean (-5.02 to -6.06), evens +0.26 mean (+0.16 to +0.35) | 6 of 18 | 4 of 18 |
| offset (odds trackoffset 15, +5.80 cent, a stand-in chosen to fit) | 18 inside; odds +0.36 mean (+0.13 to +0.68), evens +0.26 | 7 of 18 | 4 of 18 |
| bank2 (channels 4, 6 in bank 2, plus the offset) | 18 inside; evens +0.37 mean (+0.20 to +0.49) | 7 of 18 | 4 of 18 |

What the numbers say, nothing more:
- The evens jack matches in frequency without any offset (all nine peaks within +0.16 to +0.35 cent, port higher). Bank 6 and bank 2 for channels 4 and 6 are not separable (difference at most 0.15 cent).
- The odds jack is 5.0 to 6.1 cent below the hardware without an offset. The cause is unknown (M3-g); the offset variant is a fit, not an explanation.
- Width: Q mid (P3b) is 23 to 35% narrower in the port on both jacks. Q max (P3c): odds +16, +9, -7%; evens +14, +33, -3%. Q mid knob position is imprecise (M3-h), so P3b is indicative only. The 27.3 Hz odds peak in P4 is 434% wider in the port (hardware low end, M1-l, not yet examined).
- Gain after the common offset: P3b within 0.9 dB, P3c port 2.0 to 3.6 dB lower, P4 high peaks port 0.9 to 5.0 dB higher (14.5 and 17.7 kHz: +4.6 and +5.0 dB). Not interpretable without P5 (M3-e) and not examined further.
- Q min whole response (28 and 26 third-octave bands above the hardware floor): within 0.5 dB up to about 2.5 kHz, then the port is higher by +0.7 dB at 3.2 kHz rising smoothly to +6.7 dB at 16 kHz, on both jacks. 8 bands outside tolerance per jack. Cause not examined (M3-i).
- M2-b: the evens port is +0.26 cent high on average, which is the sign and about the size of the -0.32 cent that a core rate of 95,982.14 Hz would give. This is consistent with M2-b, not proof: no render at 95,982.14 Hz has been run and the odds offset is not understood. M2-c (12 kHz main loop) cannot be judged from this data.

Nothing here is a statement that the port matches the SMR. Frequencies pass only with an unexplained offset on one jack; widths and gains do not pass.

### Third part: M2-b, odds offset, Q fit, high-frequency tilt (CI run 40457983850, commit 9c26668, job port-fit)

Bryn's answers (2026-10-26): odds Nudge knob fully counter-clockwise (0%); Q mid knob at 12 o'clock by his best estimate (50%); P5 loopback not recorded.

How it was measured: `smr_render --core-rate 671875/7` runs the core's tick scheduler at 95,982.142857 Hz and processes the 96 kHz file sample for sample (no resampling, no change in `src/`). The port's frequencies and widths are then multiplied by 95982.142857 / 96000 (`compare_port.py --port-rate`, `port_fit.py`). Same measurement code as for the hardware. Saturation events and loop-guard trips over all 32 renders: 0.

1. **M2-b (core rate).** Evens jack, no offset, port minus hardware: +0.26 cent mean (+0.16 to +0.35) at 96,000 Hz (run 40454798347); -0.06 cent mean (-0.16 to +0.03) at 95,982.14 Hz. The change is the -0.32 cent the rate predicts. Frequencies: 18 of 18 inside 1 cent with the odds offset (odds +0.03 mean, -0.20 to +0.36). Consistent with M2-b and better than 96,000 Hz by about 0.3 cent against a scatter of about 0.1 cent. It assumes the Scarlett's own clock is at 96,000 Hz (not measured).
2. **Odds offset (M3-g).** Sweep of the odds trackoffset at the hardware rate, port minus hardware, odds jack mean: 11 to 13: -5.80 cent; 14: -2.89; **15: +0.01 (range -0.14 to +0.23)**; 16: +2.89; 17: +5.82. Evens (trackoffset 0, control): -0.07 for every row. So the odds jack is exactly two `exp_1voct` steps (5.80 cent) above the port's default, to within 0.03 cent. The audio cannot say whether the two steps come from the unpatched odds Freq-jack ADC reading or from a stored `trackoffset[0]`: both enter `freq_jack_cv` the same way (`params.c param_read_freq`; `system_mode.c` loads `trackoffset` from flash). The Nudge knob explanation needs about 185 ADC counts of nudge at the knob's minimum (5.8 cent, `f_nudge_odds = 1 + t_fo/55000`); with the knob fully counter-clockwise and the start-up sleep window of 800 counts (`sleep_range`) that is not what the source predicts, but the ADC floor of the knob was not measured.
3. **Q mid (M3-h).** Port width against Q pot count (`filter.c`: the first filter reaches its maximum Q at noon, `qval_a = qc * 2` capped at 4095; the second filter stays at `qval_b` 1000 until `qc` reaches 3900): odds 437 Hz peak, 1900: 1.155 Hz, 2000: 0.956, 2048: 0.874, then flat at 0.872 up to 3900. The hardware P3b widths (1.134 to 1.312 Hz) are reproduced by a Q count of 1876 for all six peaks (45.8% of travel, 172 counts or 4.2% of the range below the 2048 the port was given): widths +5.8, -7.1, -8.8 (odds, 219, 437, 875 Hz) and +4.3, +1.0, +5.3 (evens, 409, 523, 1046 Hz); 6 of 6 inside 10%; gain spread 0.83 dB. The count is pot plus Q CV (`param_read_q`: `qval_goal = qvalcv + qvalpot`); the port assumes the Q CV jack contributes 0. Per-peak counts range from 1832 to 1901, because the hardware odds widths grow with frequency while the port's are constant.
4. **Q max (new, M3-k).** At count 4095 the port's widths are 0.580 to 0.616 Hz; the hardware's are 0.515, 0.554, 0.623 (odds) and 0.521, 0.457, 0.616 Hz (evens). Width, port minus hardware at 4095: odds +15.8, +8.6, -6.8 %; evens +13.9, +34.9, -1.6 % (3 of 6 inside 10%); gain spread 1.57 dB (run 40458555879, commit 36b0044, which repeated the sweep with the same numbers). Four of the six hardware peaks are narrower than the narrowest width the port gives at any Q count in the sweep. No pot count reproduces them. Not explained.
5. **High-frequency tilt (M3-i).** Q min, port minus hardware per third-octave band (28 odds bands to 16 kHz, 26 evens bands), rms residual after fitting one constant: first-order low-pass 0.16 dB (corner 7.6 kHz); free two-corner shelf 0.08 dB odds, 0.05 dB evens (pole 6570 and 6518 Hz, zero 24547 and 23984 Hz); a 50 us / 15 us shelf at its 48 kHz corners (3183 Hz, 10610 Hz) 1.16 and 1.17 dB; the same shelf at twice those corners (6366 Hz, 21221 Hz) **0.09 dB odds, 0.07 dB evens**; a straight line against log frequency 1.26 and 1.23 dB. P4 peaks above 5 kHz against the doubled-corner shelf: residuals within 0.24 dB (odds, 7.06 and 14.5 kHz) and 0.82 dB (evens, 13.5 and 17.7 kHz; the 13.5 kHz evens peak is 60% wider in the port). Source: `codec.c w8731_init_data` Reg 05 sets `DEEMPH_48k` (DAC de-emphasis) and Reg 08 is commented "Normal, 256x, 48k ADC/DAC" while the firmware runs at 96 kHz. The WM8731 datasheet text available to me confirms the DEEMP bits select 32, 44.1 or 48 kHz de-emphasis; it does not state the time constants, so the 50 us / 15 us values are my recollection, not checked. The fit says the shape is a first-order shelf with those corners doubled; it does not prove the cause.

Not confirmed: the cause of the Q max widths; whether the odds offset is calibration or ADC; the de-emphasis time constants; the absolute level (M1-n).
Note: in run 40457983850 the Q-fit annotation was cut off at the annotation size limit; `port_fit.py` now writes one annotation per pass and run 40458555879 gave the full P3c lines.

### Fourth part: core rate 95,982.14 Hz, noise test for the Q max widths (CI run 40461296803 analysis and 40461296936 build, commit 9110ee9)

Bryn's answers (2026-10-26): core rate to 95,982.14 Hz: yes; keep trackoffset default 0, use 15 only in comparisons: yes; codec de-emphasis in Phase 1: "yes, if you recommend" (my recommendation: later, see M3-i).

Changed (`src/core` and `src/host` only, not `src/plugin`): `Constants.h` core rate 671875/7; `Resampler.h` `ratioToCore` limit 30000; `BlockAdapter` gets `prepareOneToOne`; `smr_render --core-rate` uses it; tests updated. `CMakeLists.txt` has the option `SMR_FP_CONTRACT`.

CI confirmed (all jobs success): build-macos, core-macos, oracle-linux, analysis-selftest, analyse-capture, port-fit, compare-port, port-noise. `smr_core_tests` 64 checks and `smr_param_tests` 93 checks pass on arm64 and x86-64. V4: identical fixed-render hash on arm64 and x86-64, new value **fee74c946b3031c0** (was 7f50d4657b6d1e1c; changed because the rate changed). V6 controls and script render at 44100, 48000 and 96000 Hz, mono and stereo: PASS.

Default (resampled) path against capture-1, trackoffset 15: frequencies 18 of 18 inside 1 cent (odds mean +0.03 cent, evens -0.06); widths 6 of 18 inside 10 % (at the nominal Q counts 2048 and 4095); gain 4 of 18 (the high-frequency tilt, M3-i, and Q max). Same numbers within 0.1 cent as the one-to-one render.

M3-k (Q max widths): see open-questions.md M3-k for the findings. Short form: Q-lock, firmware 5.0 vs 5.1.x and rounding are ruled out; the hardware width estimate is noise-limited on this recording; input noise at the measured floor level brings the port's Q max widths into the hardware range (rms z 1.17), but this is not confirmed as the cause and the Q count 1876 fit is not improved by it.

Not confirmed: plugin load time and memory on the Macs with the larger resampler tables; the cause of the Q max widths.

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

## Milestone 5: Plugin DSP match. CLOSED 2026-10-27 (round 1 matches; round 2 skipped by Bryn; M5-d accepted, M5-e keep flush)

Milestone 4 already put the core in the plugin at the firmware rate with resampling, mono handling, the reported latency and the denormal flush. Milestone 5 so far tests those with the real plugin code run headless. Decisions and findings: docs/open-questions.md M5-a to M5-f. Nothing in `src/core`, `src/host` or `src/plugin` was changed.

Confirmed by CI (new: `tests/plugin_tests.cpp`, a JUCE console app built from `src/plugin/PluginProcessor.cpp`; step "Build and run the Milestone 5 plugin tests" in build.yml):
- `smr_plugin_tests selftest`: 33 checks, 0 failed, on arm64 and on x86_64 under Rosetta (build run 40467599259, commit 94d3d31; repeated on 83e42c8, run 40469463502). The plugin equals a reference built from `BlockAdapter` and `ParamApplier` bit for bit in every scenario listed in M5-a. Denormal flush: 0 of 4,800,000 samples differ in a 100 s Q max run, the output reaches exactly 0 (M5-e).
- Plugin against hardware, same measurement as the core (analysis run 40467599121, job compare-plugin, plugin run at the capture-1 state with the Freq (odds) parameter at ADC count 15): frequency 18 of 18 inside 1 cent (odds mean +0.03 cent, evens -0.06), width 6 of 18, gain 4 of 18, Q min tilt +6.7 dB at 16 kHz. These are the same numbers as the core's default path (run 40461296803). The plugin output is bit-identical to the core's render after a one-sample shift (the plugin trims the reported latency, `smr_render` the rounded one): 11,039,999 of 11,039,999 samples identical in all eight files (M5-c).
- Reported latency 26 to 83 samples (22050 to 192000 Hz); not independently measured (M5-b).
- The comparison tool for Logic bounces `tools/analysis/compare_logic.py` works on fake bounces (shifted by 37 samples, quantised to 24 bit, one at -3 dB; workflow logic-compare.yml, run 40469463507): lag +37 found, gain -3.010 dB found, difference after fitting the gain -100.0 to -106.6 dB re signal, which is the 24-bit rounding level a perfect bounce would show. It validates the tool only.

Not confirmed: the plugin in Logic on either Mac at this milestone; the hardware's state cannot be set from the plugin window (M5-d, decision needed); every difference from the hardware listed under M3 is unchanged.

### Round 1 result (M4 mac mini, Tahoe, Logic 11.2, 96 kHz; release m5-logic-1, files M5_M4mini_96000_defaults and _Qmax; analysis 2026-10-27)
- Status line in Logic: "Host 96000 Hz, latency 51 samples, underruns 0". Plugin loaded from /Library/Audio/Plug-Ins/Components (AU only).
- Logic's files are named `...wav.wav` (hidden extension added a second .wav); logic-compare.yml now strips it. The release was published as a normal release, not a pre-release (harmless).
- The bounces have identical left and right channels: Logic ran the plugin mono-in, mono-out, which sums both output slots (`PluginProcessor.cpp processBlock`, nOut == 1; M1-m, M4). Both files are 256 s long (24,576,000 frames), mostly silence after the 42 s of signal.
- CI compare-logic (runs 40513849778 and 40517268619): against the render's two channels summed, lag 0, gain +0.003 dB (defaults) and +0.001 dB (Qmax), difference -42.5 and -41.3 dB re signal at whole-sample alignment.
- Cause of the -42 dB: the core's output depends slightly on where the file starts relative to the core's clock (M5-g). Checked locally with a core-only `smr_render` (g++, not the CI binary): with 525 silent samples placed before the input file, the match to the Logic bounce is -101.4 dB (defaults) and -101.8 dB (Qmax) re signal, the 24-bit rounding level. Whole-sample offsets of 0 or 2000 give -42 and -36 dB. 525 is Logic's own start offset in this project; I did not find out why it is 525.
- So for these two files the plugin in Logic equals the core's render to the 24-bit bounce limit. Not checked: 48 kHz, the Intel Mac, Logic 10.7.8, any control other than Q.
Decisions (Bryn): M5-d accept CI plus Logic default match; M5-e keep the flush; round 2 skipped. Installer now writes to /Library/Audio/Plug-Ins/Components (untested). Next: Milestone 6 (GUI v1).

## Milestone 6: GUI v1. Started 2026-10-27 (6a code written, NOT yet confirmed)

- Installer (`tools/install/Install-Unofficial-SMR.command`, build run 40519583006, commit d570345): Bryn used it on 2026-10-27 and the plugin then loaded in Logic (screenshot: Logic, host 44100 Hz, latency 33 samples). Bryn later said that first install was on the Intel Mac (I had written "M4 mac mini" without his saying so; corrected). Not recorded: what macOS asked, the script's last line. The installer is not yet tested on the M4 mac mini.
- Decisions: docs/open-questions.md "Milestone 6 decisions". Split into 6a (layout and controls) and 6b (displays).
- 6a written: `src/plugin/PluginEditor.*` (sketch v3 layout, controls bound to the existing 41 parameters, nothing new), `src/plugin/SmrUi.h`, new check `testEditor` in `tests/plugin_tests.cpp`. `src/core`, `src/host` and the parameter table are not changed.
- Not confirmed until CI is green and Bryn has run docs/testing-milestone-6a.md on both Macs: that the code builds, the window, every control, the trigger pulses (M6-c), resize in Logic 10.7.8.
- CI runner (2026-10-27): the 6a build (run 40524339534, commit 943ac80) was cancelled three times with "The job was not acquired by Runner of type hosted" (GitHub macOS arm64 capacity), so the 6a code has not been compiled by CI at that commit. `build.yml` jobs core-macos and build-macos now use `macos-26` instead of `macos-15` (analysis.yml and logic-compare.yml still use macos-15). A different image means a different Xcode and SDK than the build Bryn installed on 2026-10-27 (run 40519583006). Whether the macos-26 build runs on the Intel Mac (macOS 12.7.8, Logic 10.7.8) is not known until Bryn tests it; CI checks only the 12.0 minimum in the binary headers.
- CI result for 6a (build run 40571781905, commit 574520d, runner macos-26; all jobs success): the plugin builds as AU and VST3, universal, minos 12.0 on both slices; `auval` exit 0; `smr_plugin_tests selftest` arm64 37 checks, 0 failed, including the four editor checks (editor builds, opens at 900 x 742, accepts a resize to 1600 x 1318, is destroyed cleanly). The x86_64 (Rosetta) plugin run exited 0 (the step fails otherwise) but its check count is not visible: GitHub shows at most 10 annotations per step. `smr_param_tests` 93 checks; core tests 64 checks on arm64 and x86-64; fixed-render hash fee74c946b3031c0 on both, the same value as under macos-15 and on Linux, so the runner change did not change the core's output. The editor check labelled "resizes to the largest and smallest allowed sizes" only checks the final 1600 x 1318 size, not the minimum. Earlier attempt on commit 9d5f25d failed to compile (`runDispatchLoopUntil` in the new test, removed in 574520d).
- Still not confirmed: how the window looks and behaves, every control, the trigger pulses (M6-c), the build on the Intel Mac (macOS 12.7.8) now that it is made with a newer Xcode and SDK than the build installed on 2026-10-27.
- Bryn's first look at 6a (2026-10-27, **Intel Mac** (Bryn's correction; I had written M4 mac mini), build run 40571781905 installed with Install-Unofficial-SMR.command; informal clicking around, not the docs/testing-milestone-6a.md sheet; macOS and Logic versions not stated, expected 12.7.8 and 10.7.8): installer worked; window "looks and sounds great", very clean, re-scales perfectly. Not tested: the sheet's steps (triggers, fast click sequences, centre knob, save and reopen); the M4 mac mini with this build (Logic 11.2).
- 6a test sheet, steps done so far (Bryn, 2026-10-27, **Intel Mac**, build run 40571781905; the M4 mac mini has not tested this build): status line "Host 44100, latency 33 samples, underruns 0" (latency matches the M5-b table at 44100 Hz). Six level sliders: all six change their band's level, show the value while dragged, and double-click returns to the top. Lock buttons: all six light and unlight; with Lock 1 on, Rotate trig (right) moves the other bands and leaves the locked band alone; three locks at once also behaved as the module does (Bryn's ear, no measurement). Not yet done: knobs (Res Q, Morph, Spread, both Freq nudge, both Freq), Rotate trig fast clicks (M6-c), centre knob, switches, dropdowns, Scale buttons, save and reopen, window size limits, the M4 mac mini.
- 6a test sheet, knobs (Bryn, 2026-10-27, **Intel Mac**, build run 40571781905): Res (Q), Morph, Spread, Freq nudge (left and right), Freq (left and right) all change the sound and double-click returns each to its starting value; "no issues, all worked as expected" (Bryn's ear, no measurement). M4 mac mini not tested.
