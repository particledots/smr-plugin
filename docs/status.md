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
- 6a test sheet, Rotate trig (Bryn, 2026-10-27, **Intel Mac**, build run 40571781905): right button: every click changes the sound. Left button: not every click seems to register (one right + one left needed two lefts; 10 right + 10 left needed 11 to 13+ lefts; "hard to tell for sure"). Headless simulation of the same pulses shows the core and `ParamApplier` symmetric (docs/open-questions.md M6-c result). Added a temporary sent/seen trigger counter to the window to find where left clicks are lost; code written, not yet built by CI, not tested by Bryn.
- Trigger counter result (Bryn, 2026-10-27, **Intel Mac**, build run 40601297437): 10 clicks per button: Rot CW sent 10 seen 6, Rot CCW sent 10 seen 8. Pulses are lost before the audio side on both buttons. Code written to hold each trigger until the audio side has counted it (docs/open-questions.md M6-c, "Change"); simulated with 10 of 10 seen; not yet built by CI, not tested by Bryn.
- Trigger fix result (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975): 10 clicks per button at about 3 per second: Rot CW sent 10 seen 10, Rot CCW sent 10 seen 10; no click felt slow. Whether the sound returned to its start after 10 right + 10 left was not stated. M6-c is confirmed fixed on the Intel Mac for this pace only (not fast clicking, not the centre knob, not the Scale buttons, not the M4 mac mini). The temporary sent/seen line is still in the window.
- 6a centre knob (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975): turning the knob changes the sound step by step; sent/seen counters Rot CW 32/32 and Rot CCW 32/32 after the knob test. Audible clicks on each step with Morph at 0; they disappear with Morph up. Consistent with the manual (Morph section: at minimum the cross-fade is "almost instant, a few microseconds"; Rotate knob: with Morph at 0% the SMR "will instantly jump from one position to the next"); loudness not compared with the real SMR. Not reported: the mouse wheel over the knob and its direction. Manual also says "Reversing direction clears the queue", which matches `rotation.c rotate_down()/rotate_up()` (relevant when clicks queue under slow Morph).
- 6a centre knob wheel (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975): mouse wheel over the knob changes the sound in both directions; wheel up turns the knob's line clockwise (matches `RotateDial::mouseWheelMove`, up = Rotate CW) and the sound steps in the same direction as the right Rotate trig button. M4 mac mini not tested.
- 6a switches (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975): the five two-position switches slide on every click. The three-position Fast | Trigger | Slow switch reaches all three positions, but you must click at the position you want (the dot goes where the mouse points; `SlideSwitch::mouseDown`). It was on Fast when Bryn started this step; the plugin default is Slow (`ParamMap.cpp` envspeed default 2, M4-d). Not yet known whether that was an earlier click or saved project state, or a fault in the window; a fresh instance has not been checked. The switches' effect on the sound is not tested (Milestone 7).
- 6a Env speed default (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975): after reloading the plugin the Fast | Trigger | Slow switch opens on Slow, the plugin default (M4-d). The earlier Fast was probably Bryn's own earlier click (his words: "i may have clicked before doing the test"); resolved. The starting positions of the other five switches were not reported.
- 6a switch defaults on a fresh launch (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975): 1.3.5 | 1, 2.4.6 | 6, CV slew, Scale rotation on the left; Pre | Post on the right (Post); Fast | Trigger | Slow on Slow. This matches the parameter defaults in `ParamMap.cpp` (oddsmode 0, evensmode 0, slew off, scalerotation off, prepost 0 = Post, envspeed 2 = Slow).
- 6a dropdowns and Scale buttons (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975): Bank box changes the sound; Two-Pass / One-Pass changes the sound each time; the filter type box changes the sound each time; the two Scale buttons change the sound "dramatically each time". Trigger counters after the Scale test: Scale up 6/6, Scale down 5/5 (sent/seen; more clicks than the three each I asked for, none lost). Not recorded: which bank and filter type names were used. M4 mac mini not tested.
- 6a save and reopen (Bryn, 2026-10-27, **Intel Mac**, build run 40603653975; per the project's Mac note the Intel Mac is macOS 12.7.8 with Logic 10.7.8, not re-checked today): moved slider 2 to about half, Res (Q) up, CV slew on, a different Bank, three Rotate trig clicks; saved, closed and reopened the Logic project: slider 2, Res (Q), CV slew and Bank all came back as left ("yes to all"), and the rotated pattern sounded the same (his ear, no measurement; the answer to the rotated-pattern question was a blanket yes). Not tested: the M4 mac mini; saving with locks on.
- 6a window size (Bryn, 2026-10-27, **Intel Mac**, Logic 10.7.8 per the project's Mac note, build run 40603653975): the window resizes; smallest about a quarter of the screen, largest about 80% of the screen ("very big"), everything stays in proportion (aspect ratio held, nothing reported cut off or overlapping). Pixel sizes not measured. So Logic 10.7.8 honours the resize limits (M6-e), at least in this respect.
- 6a status after this session: on the Intel Mac the sheet is done (sliders, locks, knobs, Rotate trig incl. the M6-c fix, centre knob and wheel, switches and defaults, dropdowns, Scale buttons, save and reopen, window size). Open: the whole sheet on the M4 mac mini (Logic 11.2); Q1 and Q5 (licence) before anyone else gets a build; the temporary sent/seen line to remove; hold-and-click speed faster than about 3 per second not tested.

## Milestone 6b: displays. Started 2026-10-27 (built by CI, NOT tested by Bryn)

- Firmware reading finished (docs/m6b-led-notes.md "Second pass"): `envout_pwm.c` (all), `audio_util.c audio_convert_2x16_to_stereo24` (the input clip loop), `filter.c` slider clip, `leds.c update_slider_LEDs`, `params.c TIM1_UP_TIM10_IRQHandler`, `led_ring.c display_scale` (ring geometry), `main.c` loop.
- Decisions (Bryn, 2026-10-27): M6-f no scale or bank view on the ring; meters as peak bars with a short hold; slider LED at the literal firmware ratio. Other choices and placeholders: docs/open-questions.md M6-g to M6-j.
- Code: `src/plugin/DisplayModel.h` (new), `PluginProcessor.*` (publishes a read-only copy after each host block; audio calls unchanged), `PluginEditor.*`, `SmrUi.h` (slider LED), `tests/display_checks.h` (new) and `testDisplay` in `tests/plugin_tests.cpp`. `src/core`, `src/host` and the parameter table are not changed. The "triggers sent/seen" line is kept until Bryn says the M4 mac mini 6a test is done.
- CI result (build run 40609185410, commit 1728ab4, runner macos-26; all jobs success): the plugin builds as AU and VST3, universal, minos 12.0 on both slices; `auval` exit 0; `smr_plugin_tests selftest` on arm64: 59 checks, 0 failed, including the 14 new display checks and the unchanged bit-for-bit checks (plugin output does not depend on host buffer size; plugin equals reference at default and at Q max); `smr_param_tests` 93 checks. The first run (940a992) failed one display check that assumed `ENVOUT_PWM` is at most 4095; `envout_pwm.c` allows more (Post is `>> 16`), the largest value in the noise test was 5752, and the check now reports it instead.
- Not in CI output: the x86_64 (Rosetta) check count (annotation limit), how the window looks, any light or meter, the clip thresholds against real signals.
- Not confirmed: how it looks, every light, the meters, the M4 mac mini. Test sheet: docs/testing-milestone-6b.md. The 6a build is still untested on the M4 mac mini.

- 6b test sheet, sections 1 and 2 (Bryn, 2026-10-27, **Intel Mac**, build run 40609185410; the M4 mac mini has not tested this build): section 1, all items present and correct (ring, six Env out lights, clip LED, four dashed meters, slider LEDs, the sent/seen line). Ring at start: Bryn's screenshot (received afterwards, no audio playing): blue at 3 o'clock, then teal, green, grey-white, orange and a red-looking dot at the bottom, going clockwise, 6 lit dots on consecutive positions, the other 14 dark; the six Env out lights, slider LEDs, clip LED and the four meters are dark/empty. Bryn says positions and colours are correct. The channel 6 dot (firmware 766,0,112) reads red rather than pink on screen. Rotate trig right: the pattern moves counter-clockwise on the ring, and Bryn says the SMR moves the same way. Morph up plus Rotate trig: colours slide from one dot to the next. Slider 3 at the bottom: its ring dot goes out completely (I expected a very dim dot; see open-questions M6-h). Lock 2: about 25 to 30 flashes in 10 seconds in the window; Bryn says the real SMR flickers much faster rather than pulsing, and he accepts the slower flash. Not yet done: sections 3 (slider LEDs), 4 (Env Out), 5 (clip LED and meters).

### 6b test, sections 3 to 5 (Bryn, Intel Mac, run 40609185410, 2026-10-27)
- 3 Slider LEDs: visible but hard to see on the white panel. A clip flash was seen when switching Two-Pass to One-Pass (not with Res/slider 1 tried). Not a failure.
- 4 Env Out lights: Fast/Slow follow bands in channel colours; Trigger jumps on about 0.1 s; Post with a slider at the bottom keeps its light dark. All as expected.
- 5 Meters: In and Out bars move, hold line about 1 s, fall to empty when stopped. As expected. Clip LED level (about -3 dBFS) and negative-only input: not tested by Bryn (CI selftest covers negative input: testDisplay, -0.9 does not light the LED).
- Complaint: ring colours low contrast on the white panel. Addressed in the next build (see open-questions M6-h addendum 2).

### 6b ring contrast and idle ping (Bryn, Intel Mac, 2026-10-27)
- Build 40625336120 (attempt 2; the first download was blocked by a virus warning and a re-run fixed it): ring "looks better", all good (Bryn's words).
- Idle ping on the filters (about one every 30 to 45 s with no audio) seen once in an older project, not seen in a new project. Not reproduced, cause unknown. Offline check: 5 minutes of silence through the core (default settings and sliders up with high Q) gave output exactly 0 (local g++ build of tools/smr_render, not a CI result). If it comes back, test the input path first.
- 6b accepted on the Intel Mac. Not tested on the M4 mac mini. The "triggers sent/seen" line stays.

## Milestone 7, step 1: Lock buttons (started 2026-10-27, NOT built by CI yet, NOT tested by Bryn)

- Scope decisions (Bryn): in = Q lock, Lock jacks (2 trigger params), transposition and fine tune, forbidden notes, 1V/oct (lights only, a fourth Env speed position); out = System Mode, settings slots, colour schemes, user-scale editing. AU stays `aufx`. Lock hold time follows the firmware count (M7-a).
- Code: `src/core/Engine.cpp process_lock_buttons` (port of `params.c`, PLAY branch), `Constants.h kLockButtonQunlockHoldTime`; six new host parameters `lockhold1..6` (47 in total, appended at the end); `ParamApplier::apply` sets the Lock pins every block and `coreLockChanges` reports core-made lock changes; `SmrAudioProcessor::syncLockParameters` (30 Hz timer, message thread) copies them into the Lock parameters; `SmrUi.h LockButton` (tap, shift-click hold, Q-lock flicker, sticky flash).
- V1 (oracle) extended with the Lock button pins (stimulus magic SMRSTIM2, Step 197 bytes). New finding P-10 (out-of-range table read in the firmware when BpRe is selected with a channel on bank 19); the stimulus avoids that switch.
- Local pre-checks only (not CI): `smr_core_tests` 73 checks, `smr_param_tests` 98 checks, no failures; V1 8 of 8 scenarios identical; fixed-render hash unchanged (fee74c946b3031c0). The JUCE code (LockButton, sync timer, `testLockHoldSync`) has not been compiled yet: JUCE is built by CI only.
- Test sheet: docs/testing-milestone-7a.md.
- CI result (build run 40632430642, commit deea910; all three jobs success: core-macos, build-macos, oracle-linux): plugin builds as AU and VST3, universal, minos 12.0 on both slices; `auval` exit 0; `smr_core_tests` 73 checks and `smr_param_tests` 98 checks, 0 failed on linux, arm64 and x86-64; fixed-render hash fee74c946b3031c0 identical on arm64 and x86-64 (V4); `smr_plugin_tests selftest` on arm64 62 checks, 0 failed, including the new lock hold sync checks; V1 scenarios 0 to 7 identical, V1a tables identical, V1 negative control detects a change. The Capture analysis (Milestone 3) run was still queued when this was written. Not confirmed: how the window looks and behaves; the x86_64 (Rosetta) plugin check count (annotation limit); anything on a real Mac.

### Milestone 7 step 1 test (Bryn, **Intel Mac**, build run 40632430642, 2026-10-27; the M4 mac mini has not tested this build)
- Sheet sections 1 to 3 and 4 passed. Tap toggles correctly (including five quick clicks). Shift-click hold, Q lock (Q stays on channel 3 when Res (Q) is dragged further), release after a Q turn does not toggle, flicker about every 2 s (matches 24016 passes at the 12 kHz placeholder, about 2.0 s), long hold unlocks and keeps the Q lock, two taps clear it. Lock 1 parameter in Logic automation reads ON while lit.
- Hold time, stopwatch (Bryn, screenshot): 3.85 s from the shift-click to Lock 4 going dark. Offline core measurement 4.167 s (50000 passes at 12 kHz, local build, not CI). The 0.3 s difference is within start/stop error by hand; his first count (2 to 3 s) was a hand count. M7-a stands at the 12 kHz placeholder.
- Open: while shift-held, the inner ring on the Lock button stays visible instead of flashing (Bryn, step 2.1); the hold itself works. Cause not found. Not yet tested: the M4 mac mini.

## Milestone 7, step 2: Lock jacks (started 2026-10-27, built by CI, passed on the Intel Mac only; M4 mac mini NOT tested)

- Firmware read (docs/open-questions.md M7-f): `params.c process_lock_jacks` inverts `lock[0]` (and `lock[2]`, `lock[4]` when `mod_mode_135 == 135`) on `do_LOCK135`, and `lock[5]` (and `lock[1]`, `lock[3]` when `mod_mode_246 == 246`) on `do_LOCK246`; nothing else. `dig_inouts.c debounce_trigger_inputs` sets the flags at the end of a pulse, the manual says rising edge (M7-f, not resolved).
- Code: two new trigger parameters `lockjack135` and `lockjack246` (49 in total, appended at the end; `ParamMap.h/.cpp`, `Role::LockJackOdds`, `Role::LockJackEvens`); `ParamApplier::apply` calls `Engine::triggerLock135()` and `triggerLock246()` on a rising edge; trigger counters extended from 4 to 6 (`PluginProcessor`, `Status::trigSeen[6]`); window: "Lock odds" and "Lock evens" buttons under the Freq knobs (`PluginEditor.cpp`), the temporary sent/seen line shows six triggers. `src/core` is not changed: `Engine.cpp process_lock_jacks` and the V1 events for the jacks already existed.
- New tests: `testLockJacks` in `tests/core_tests.cpp` (12 checks), Lock jack checks in `tests/param_tests.cpp` (12 checks, count now 49), `testLockJackSync` in `tests/plugin_tests.cpp` (through the real processor, includes the sync into the Lock parameters and the counters).
- Local pre-checks only (g++ on Linux, not CI): `smr_core_tests` 85 checks and `smr_param_tests` 110 checks, 0 failed; fixed-render hash fee74c946b3031c0 (unchanged). The JUCE code (editor buttons, processor, `testLockJackSync`) has not been compiled yet: JUCE is built by CI only.
- Test sheet: docs/testing-milestone-7b.md.
- CI result (build run 40641849028, commit 7ad84ab; all three jobs success: core-macos, build-macos, oracle-linux): plugin builds as AU and VST3, universal, minos 12.0 on both slices; `auval` exit 0; `smr_core_tests` 85 checks and `smr_param_tests` 110 checks, 0 failed on linux, arm64 and x86-64; fixed-render hash fee74c946b3031c0 identical on arm64 and x86-64 (V4); `smr_plugin_tests selftest` on arm64 67 checks, 0 failed, including the new lock jack checks; V1 scenarios 0 to 7 identical and the V1 negative control detects a change. The first run on commit de3a799 (run 40641672409) failed to compile `tests/plugin_tests.cpp` (my `fmt` call had six values, the helper takes four); fixed in 7ad84ab, nothing else changed. Not confirmed: the x86_64 (Rosetta) plugin check count (annotation limit), how the window looks and behaves, anything on a real Mac. The Capture analysis (Milestone 3) run was still in progress when this was written.

### Milestone 7 step 2 test (Bryn, **Intel Mac**, build run 40641849028, commit 7ad84ab, 2026-10-28; the M4 mac mini has not tested this build)
- Sheet docs/testing-milestone-7b.md, all sections matched, nothing failed.
- Start state (switches at 1.3.5 and 2.4.6, all six Lock buttons dark): yes.
- 1 Lock odds: Lock 1, 3, 5 light together and go dark on the second click; 2, 4, 6 stay dark. No sound change heard (as expected with no input-dependent check; not an error).
- 2 Lock evens: Lock 2, 4, 6 light and go dark together.
- 3 Switches: with the left switch at "1" only Lock 1 toggles; with the right switch at "6" only Lock 6 toggles.
- 4 Each channel inverts on its own: with Lock 3 lit, Lock odds darkens 3 and lights 1 and 5; again gives back 3 lit, 1 and 5 dark.
- 5 Locked channels stay put: with 1, 3, 5 locked, three Rotate trig (right) clicks moved 2, 4, 6 and left 1, 3, 5 in place (by ear).
- 6 Fast clicking: six quick clicks on Lock odds, "Lock 135" sent/seen 36/36, Lock 1, 3, 5 ended dark.
- 7 Save and reopen: with 2, 4, 6 lit, after save, close and reopen of the Logic project 2, 4, 6 were still lit and did not toggle at load; Lock evens once darkened them.
- 8 Automation list: "Lock 1 3 5 (trigger)" and "Lock 2 4 6 (trigger)" are both listed.
- Not tested: the M4 mac mini. Still open: M7-f (rising edge versus firmware acting after the pulse), the step 1 cosmetic (inner ring on a shift-held Lock button does not flash). The temporary "triggers sent/seen" line stays until Bryn says the M4 mac mini 6a test is done.

## Milestone 7, step 3: fine tune and transposition (started 2026-10-28; no code change yet)

- Firmware and manual read (docs/open-questions.md M7-g): `params.c param_read_freq` (fine tune `1 + t/55000`, 13-zone transposition while `lock_pressed[j]`, clear by `lock_down[] > 36000`), manual p.11-12. The SMR has no separate Transpose jack; transposition is Lock button held plus the Freq Nudge knob of that side.
- The audio is already in the core (M2 port, V1 oracle covers it) and the host parameters exist (`nudgeodds`, `nudgeevens`, Lock hold). Nothing in the plugin has been listened to for this yet.
- Step 3a (now): listening test on the existing build, sheet docs/testing-milestone-7c.md. No new CI run.
- Step 3b (after the result): the Env Out light display for fine tune and transposition and the Lock button flash while a clear is staged (display state omitted from the core in P-8).
- The temporary "triggers sent/seen" line stays until Bryn says the M4 mac mini 6a test is done.

### Milestone 7 step 3a test (Bryn, **Intel Mac**, build run 40641849028, 2026-10-28; the M4 mac mini has not tested this)
- Matched as described in docs/testing-milestone-7c.md: 2.4 and 2.5 (pitch stays at +3 after release, Lock 1 not lit; stays +3 with Nudge at 0%), section 3 (no change when wiggling outside the saved zone; follows again at the saved zone, about 73%), section 4 (odds 1, 3, 5 and evens 2, 4, 6 move together, the other side not), section 5 (locked channel 5 transposes and stays lit), section 6 (Bryn: "all worked"; his message did not give the answer to 6.2, whether a 2 second hold cleared the transpositions and whether any Lock lit).
- 2.3: 12 steps heard going down from the top. 2.2 and 2.3 together: the tuner note swept one octave, F# at 0% to the next F# at 100%, which fits -6 to +6 semitones (the tuner shows note names, not 600 cents).
- **Mismatch 1 (open):** 2.2 going up from 0%, the first step came at about "9 o'clock" and Bryn counted 9 or 10 steps (hard to tell). The sheet expects the first step in the middle (46 to 54%) and 6 steps. One possible cause is a transposition left on channel 1 from earlier use of the same plugin instance (the zone to cross is `saved_coarse_adj`), not yet checked.
- **Mismatch 2 (open):** 1.2 fine tune by ear: C# at 0%, D at 50%, D# at 100%, "about a semitone more on each click". Expected from the source: about +63 cents at 50% and +124 cents at 100%. Not measured in cents yet.
- Follow-up test sheet sent in chat (2026-10-28): fine tune in cents at 0, 25, 50, 75, 100%; and 2.2 again on a fresh plugin instance.

### Milestone 7 step 3a follow-up (Bryn, **Intel Mac**, same build, fresh plugin instance, 2026-10-28)
- Fine tune in cents (channel 1, no Lock, tuner): 0% = C; 25% = C about +35 cents; 50% = C# about -35 cents (about +65); 75% = C# (about +100); 100% = C# about +30 cents (about +130). Source expectation: +32, +63, +94, +124 cents. Agrees within what the tuner reading allows. The earlier "D then D#" in step 1.2 was a by-ear misread; mismatch 1 is closed.
- Transposition going up from 0% on a fresh instance: first step between noon and 1 o'clock, 6 steps. Matches the source. Mismatch 2 is closed; the earlier early start fits a transposition left on channel 1 from earlier use of that instance (not proven, but the fresh instance behaves as the firmware does).
- Not answered: 6.2 (does a 2 second hold of all six Locks clear the transpositions, and does any Lock light). Only bears on M1-c (2 s in the manual, about 3 s in the source count).
- Step 3a accepted on the Intel Mac. M4 mac mini not tested. Next: step 3b (Env Out lights for fine tune and transposition, Lock flash while a clear is staged).
- Bryn, later the same day: the transposition cleared after the all-six-Locks hold (answer to 6.2: "cleared for sure"); how long he held it and whether any Lock lit he does not remember. M1-c stays open. Bryn on his real SMR (for M7-h): sweeping Evens Nudge 0 to 100 % the light goes white to blue and stays lit for a second or less after he stops; he did not say what the top 6 % does.

## Milestone 7, step 3b: Env Out lights for fine tune and transposition, Lock flash (started 2026-10-28; built by CI, ACCEPTED on the Intel Mac 2026-10-28; M4 mac mini NOT tested)

- Read first (2026-10-28): `led_ring.c calculate_envout_leds` (COARSE TUNING and FINE TUNING branches), `leds.c update_lock_leds` (line 204, `num_clear_coarse_staged`, `coarse_adj_led`), `params.c param_read_freq` display state, `params.h DISPLAYTIME_*`, manual p.11 and 12.
- **Correction:** `param_read_freq` runs in `params.c TIM1_UP_TIM10_IRQHandler`, so the display times are TIM10 ticks (about 20.5 kHz): `DISPLAYTIME_NUDGEONESIDE` 12000 is about 0.585 s, `DISPLAYTIME_NUDGEBOTHSIDES` 25000 about 1.22 s. The M7-g text that said main-loop passes (1 s, 2.1 s) was wrong; fixed in docs/open-questions.md (M7-g, new M7-h, M7-i, M7-j).
- Core (`src/core`, additive, audio path untouched): `Params.cpp param_read_freq` now carries `ongoing_fine_tuning`, `ongoing_coarse_tuning`, `fine_tuning_timeout`, `fine_timer`, `fine_envled`, `cur_envled_state`, `saved_envled_state`, `coarse_adj_led`, `old_switch_state` and the display `else if` chain (which belongs to the EVENS `if`, as in params.c); `coarseLadder` also sets `cur_envled_state`; `Constants.h kDisplayTimeNudge*`; members in `Engine.h`. docs/port-notes.md P-8 updated.
- V1: the trace now carries those variables plus `coarse_adj` (189 words per step); new scenario 8 (`tests/oracle/gen_stimulus.cpp`) freezes the Nudge controls, Lock buttons and the 135/246 switches for thousands of steps so the 12000 and 25000 tick timeouts run out; the CI loop runs scenarios 0 to 8.
- Plugin (`src/plugin`): `DisplayModel.h envOutLedsFw` (COARSE and FINE TUNING branches, `fwU16`), `lockLedUpdate` (Q-lock flicker and the staged-clear flash), new `DisplayData` fields filled by `fillDisplayData`; `PluginEditor.cpp updateDisplay` runs `envOutLedsFw` once per ring redraw (`kRingFlashMs`, rests on M2-c) and the Lock LED update once per `kLockLedUpdateMs`; `SmrUi.h LockButton::inverted` replaces `qFlicker`. Play and 1V/oct Env Out lights stay live as in 6b (M7-j).
- Provisional: the negative-brightness case (M7-h) gives 0.
- Local pre-checks only (g++ on Linux, not CI; JUCE code not compiled): `smr_core_tests` 85 checks and `smr_param_tests` 110 checks, 0 failed; fixed-render hash fee74c946b3031c0 (unchanged); V1 scenarios 0 to 8 identical, bit for bit, with the display state in the trace; JUCE-free display checks (`tests/display_checks.h`) 24 checks, 0 failed (13 new, expected values worked out by hand from the firmware source).
- Test sheet: docs/testing-milestone-7d.md.
- CI, partial (run 40709457669, commit f65a98c): job oracle-linux succeeded: `smr_core_tests` 85 checks and `smr_param_tests` 110 checks, 0 failed on linux; fixed-render hash fee74c946b3031c0 (unchanged); V1 scenarios 0 to 8 identical, 189 words per step including the display state (scenario 8: 30000 steps, no saturation); V1a tables identical; V1 negative control detects a change. The macOS jobs core-macos and build-macos were cancelled twice with "The job was not acquired by Runner of type hosted" (no free macOS arm64 runner), a third attempt was still queued when this was written. So NOT confirmed by CI: the JUCE code (`DisplayModel.h` in the plugin, `PluginEditor.cpp`, `SmrUi.h`, `tests/plugin_tests.cpp` selftest including the 13 new display checks on arm64), the universal build, `auval`. The display checks passed locally under g++ without JUCE (24 checks).
- **CI result, complete (run 40709457669, third attempt, commit f65a98c; all three jobs success: core-macos, build-macos, oracle-linux; 2026-10-28):** plugin builds as AU and VST3, universal, minos 12.0 on both slices; `auval` exit 0; `smr_core_tests` 85 and `smr_param_tests` 110 checks, 0 failed on arm64 and x86-64; fixed-render hash fee74c946b3031c0 identical on both (V4); `smr_plugin_tests selftest` on arm64 80 checks, 0 failed, including the new coarse/fine tuning and lock LED checks; V1 scenarios 0 to 8 identical. Not confirmed: how the lights look and behave on a real Mac, the x86_64 plugin check count. Bryn has downloaded the build, not yet installed or tested it (2026-10-28 evening).

### Milestone 7 step 3b test (Bryn, **Intel Mac**, build run 40709457669, commit f65a98c, 2026-10-28 evening; the M4 mac mini has not tested this build)
- Section 1 fine tune lights: behaved as described in docs/testing-milestone-7d.md. Not tested: moving both knobs at once (one mouse pointer), so the 1.22 s both-knob timeout is unchecked.
- Section 2 blinks: two blinks lasting one second or less.
- Section 3 transposition lights: exactly as described (+1/+3/+6 blue from light 1, -1/-3/-6 red from light 6, three red and three blue at zero).
- Section 6 (M7-h): the light stays blue at the top of the range, in the plugin and on his real SMR. Recorded in docs/open-questions.md M7-h.
- Section 5 (staged clear flash and the hold time, M1-c): not done yet. Bryn did not know how to hold all six Lock buttons; instructions sent in chat. Sections 2.3 (Lock-disabled dim), 4 and the "returns within 0.3 s" part of 3.4 were not separately reported.
- Step 3b section 5, staged-clear flash timing (Bryn, **Intel Mac**, same build, hand stopwatch started at the shift-click of Lock 6, stopped at the first amber flash of Lock 1): run 1 4.80 s, run 2 4.03 s. Offline core measurement (scratch program, not committed: all six `setLockButton` pressed, one `runLoopPass` and one `runTim10Tick` per step): `num_clear_coarse_staged` becomes 6 after 36000 loop passes, 3.000 s at the 12 kHz placeholder (M2-c). On top of that the window shows the flash only when `lockLedUpdate` has `flash2 == 0`: 1 to 5 updates of 0.125 s later (0.125 to 0.625 s), for one update, plus Bryn's reaction time. So the plugin should read about 3.1 to 3.8 s plus reaction. Run 2 fits; run 1 is about 0.75 s over, which hand timing cannot separate from the model. Not enough to say anything about the real SMR's 2 s versus 3 s (M1-c): this measures the plugin. Next: the same measurement on his SMR.
- Step 3b section 5.4 and remaining sections (Bryn, **Intel Mac**, same build, 2026-10-28 late evening): releasing Locks 3, 4, 5, 6 first and then Lock 1, Lock 1 stops flashing and channel 1 drops by 3 semitones (by ear). Sections 2.3 (Lock-disabled dim), 4 (several channels at once) and the 0.3 s return in 3.4 behaved as the sheet says.
- **Step 3b accepted on the Intel Mac (Bryn, 2026-10-28).** Source cross-check: `params.c process_lock_buttons` (called from the main loop, `main.c` line 156) counts `lock_down[]` in main-loop passes, so the 36000-pass clear model holds; `leds.c update_lock_leds` flashes on every fifth update (`flash2`), as `DisplayModel.h lockLedUpdate` does.
- Not confirmed: (1) both Nudge knobs at once (1.22 s): a mouse cannot move two knobs, the core logic is covered by V1 scenario 8 (bit for bit), the window path (`PluginEditor.cpp updateDisplay`) is untested; (2) staged-clear time on the real SMR (M1-c, M2-c): Bryn will measure later (three runs, stopwatch from the last of the six Lock presses to the first amber flash); plugin runs of 4.80 s and 4.03 s are hand timing and cannot separate reaction time from the model; (3) the M4 mac mini for this build.

## Milestone 7, step 4: forbidden notes (started 2026-10-28; code written, NOT built by CI yet, NOT tested by Bryn)

- Scope (Bryn, step 1): in = forbidden notes. Read first: `params.c process_freq_blocks` (`main.c` line 158), `params.c process_rotary_button` (only the `rotary_switch_b` pulse is used), `led_ring.c update_LED_ring` (grey light), `system_mode.c load_params_from_sram` (the count), manual "Forbidden Notes" printed page 16. Details and the manual-versus-source comparison: docs/open-questions.md M7-k (no disagreement found for the press; storage is still M1-h).
- Core: `Engine.cpp process_freq_blocks` (new, port of the PLAY branch), `Engine::triggerRotaryPress()`, members `num_freq_blocked` and `rotary_switch_b`; `runLoopPass` calls `process_freq_blocks()` right after `process_lock_buttons()` and clears the pulse; `setFreqBlock` and `Snapshot.cpp loadSnapshot` keep `num_freq_blocked` in step with `freqblock`. Not ported: the rest of `process_rotary_button` (docs/port-notes.md).
- Plugin: parameter `rotatepress` "Rotate Press (trigger)" (50 parameters in total, appended at the end; `ParamMap.h Role::RotatePress`, `kNumTriggers` 7), `ParamApplier::apply` calls `triggerRotaryPress()` on a rising edge; button "Rotate press" in `PluginEditor.cpp`; the temporary sent/seen line shows seven triggers. The blocked-note lights and the saving of `freqblock` already existed (`DisplayModel.h`, `StateCodec.cpp`).
- V1: scenario 9 added (`tests/oracle/gen_stimulus.cpp`): Lock pins and ROTATE press pulses in 100-step cycles; new stimulus bits in `events` (bit 4 press pulse, bit 5 firmware's own `freqblock`, bit 6 firmware's own `lock_pressed[]` and `lock_down[]`; the Step layout is unchanged); trace gains `freqblock`, `num_freq_blocked` and `already_handled_lock_release[6]` (197 words per step); the CI loop runs scenarios 0 to 9. The oracle calls the unmodified `process_freq_blocks()`; the press pulse is the stimulus.
- Local pre-checks only (g++ and gcc on Linux, not CI; the oracle built from the pinned firmware 79bc953): `smr_core_tests` 102 checks and `smr_param_tests` 117 checks, 0 failed; fixed-render hash fee74c946b3031c0 (unchanged); V1 scenarios 0 to 9 identical bit for bit (scenario 7 up to its first saturation, as before); scenario 9 coverage in the oracle trace: 138 blocks, 137 unblocks, 10 clear-alls, 10 times at 14 blocked; a port with the limit changed to 15 fails scenario 9 at step 1420 (checked, then reverted). The JUCE code (`PluginEditor.cpp`, `PluginProcessor.cpp`, `testRotatePressSync` in `tests/plugin_tests.cpp`) has not been compiled: JUCE is built by CI only.
- Test sheet: docs/testing-milestone-7e.md.
- **CI result (build run 40829856814, commit bccc09d; all three jobs success: core-macos, build-macos, oracle-linux; 2026-10-28):** plugin builds as AU and VST3, universal, minos 12.0 on both slices; `auval` exit 0; `smr_core_tests` 102 and `smr_param_tests` 117 checks, 0 failed on linux, arm64 and x86-64; fixed-render hash fee74c946b3031c0 identical on arm64 and x86-64 (V4); `smr_plugin_tests selftest` on arm64 83 checks, 0 failed, including the three new rotate press checks; V1 scenarios 0 to 9 identical (197 words per step; scenario 7 up to its first saturation, as before), V1a tables identical, V1 negative control detects a change. Not confirmed: how the button and the grey lights look and behave on a real Mac, the x86_64 (Rosetta) plugin check count. The Capture analysis (Milestone 3) run was still in progress when this was written.
- Still open from step 3b: the M4 mac mini has not tested any M6/M7 build; M1-c and M2-c (real SMR timing, Bryn will measure later); the both-knob 1.22 s window path.

