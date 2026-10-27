# Open questions

Items for Bryn to decide. Nothing here has been decided silently. Each entry says
what was read, and where.

## Licensing (found in Milestone 0)

### Q1. JUCE licence route
`LICENSE.md` of JUCE 8.0.6 says the modules are dual-licensed under AGPLv3 or the
JUCE 8 End User Licence Agreement, and that using JUCE without the AGPLv3 route
binds you to the EULA.

Tier figures below come from a JUCE forum post about the JUCE 8 licence amendments
(forum.juce.com, "Amendments to the JUCE End User Licence Agreement for JUCE 8"),
not from the EULA text itself. Check juce.com before relying on them.
- Starter: free, revenue/funding up to $20,000 per year; the forum post says the
  splash screen requirement was removed for Starter.
- Indie: up to $300,000, $40/month or $800 perpetual.
- Pro: no limit.
- For individuals the limit counts total revenue from all sources including
  donations, sponsorship and advertising (same post).

Options for a free plugin:
- A. Starter EULA route: source can stay private, binaries free to share, as long as
  you stay inside the Starter limits and its terms.
- B. AGPLv3 route: public source for the whole plugin (src/ included) under AGPLv3
  whenever binaries are shared. The MIT firmware code can be combined with it.

Decision needed before any public release. Test builds for yourself do not need
this decided.

### Q2. Licence for the code in `src/`
No licence is assigned. Default is all rights reserved. It depends on Q1
(route B forces AGPLv3 on the combined plugin).

### Q3. Kickoff pack says "4ms copyright"; the source says otherwise
The kickoff pack says to copy the MIT licence "with the 4ms copyright".
`third_party/SMR/LICENSE` reads "Copyright (c) 2015 Dan Green (danngreen1@gmail.com)".
No company name appears in it. The notices copy it exactly as written.

### Q4. Using the text "4ms" in repo files
`README.md` of the firmware repo (Guidelines for derivative works) forbids the text
"4ms" and the logo on derivative works, and names faceplates, enclosures and
front-panels. The submodule URL (`.gitmodules`) and the commit provenance in
repo docs unavoidably contain it. Current choice, change if you disagree:
"4ms" is kept out of the plugin name, UI, plugin metadata and the shipped
`THIRD-PARTY-LICENSES.txt`; it may appear in repo-internal files.

### Q5. Module panel look vs hardware licence
The firmware `LICENSE` says the hardware is CC BY-NC-SA. The GUI is meant to
resemble the module (Milestone 6). Whether copying the panel layout counts as
using the hardware design is not something I can settle. Needs your decision or
the designer's answer before the GUI is shared.

## Milestone 1 source audit: licensing answers

### Q6. GPLv3 code in the firmware tree
`third_party/SMR/rotary.c` and `rotary.h` carry "Copyright 2011 Ben Buxton.
Licenced under the GNU GPL Version 3." This is the rotary encoder decoder, a
hardware-only file. Rule for the port: it must not enter `src/core`.
**Answered (Milestone 1):** it holds only `init_rotary` and `read_rotary` (GPIO setup and the
encoder state table). Nothing in the DSP path needs it. It stays out of `src/core` and the plugin.

### Q7. ARM CMSIS headers
`filter.c` includes `arm_math.h` (and `audio_util.h` includes `<arm_math.h>`). The
copy in `third_party/SMR/stm32/core/include/arm_math.h` is headed "Copyright (C)
2010 ARM Limited. All rights reserved", CMSIS DSP Library V1.0.10, "Target
Processor: Cortex-M4/Cortex-M3/Cortex-M0". I have not read that licence's terms.
The plan to replace ARM math with a shim (Milestone 2) avoids copying it;
Milestone 1 must read the terms and list every `arm_` call actually used.
**Answered (Milestone 1):** grep finds no `arm_*(` call in any `.c` or `.h` outside `arm_math.h`.
The only references are the includes in `filter.c` and `audio_util.h`. The header has no licence
text, only "Copyright (C) 2010 ARM Limited. All rights reserved." So its terms cannot be read
from the file. We do not copy it, and the port does not need it. The ARM-specific code on the DSP
path is one `usat` asm in `envout_pwm.c` (to be replaced by a plain clamp).

### Q8. Dependency on stmlib
The firmware README says to clone a separate `stmlib` repo next to SMR. A grep of
the `.c`, `.h` and `Makefile` files in the zip for the string "stmlib" returned
no matches. So the audit has to confirm whether any stmlib code is actually
needed. Not assumed either way.
**Answered (Milestone 1):** "stmlib" appears only in the firmware `README.md`. No source file or
Makefile uses it. Not needed.

## Carried over from the kickoff pack (Milestone 1 findings)

- Fast | Slow switch Trigger position and the v5 1V/octave mode. **Answered:** Trigger is the
  centre position (`filter.c`, `envout_pwm.c`, `dig_inouts.h` `ENVSPEEDFAST`/`ENVSPEEDSLOW`).
  1V/oct mode is entered by holding ROTATE while flipping to Fast; Pre/Post then selects glissando.
- Rotate encoder press and bank selection. **Answered:** tap toggles Scale mode, hold and turn
  selects the bank (applied on release), Lock + ROTATE forbids a note (`params.c`
  `process_rotary_button`, `process_rotary_rotation`).
- Odds/evens normalling and mono input handling. **Answered:** normalling is jack switches, no
  firmware code. A mono plugin input should be copied to both input slots. Output labelling is M1-m.
- Which v5 features are in scope for v0.1. **Still yours to decide.**
- Your SMR firmware: "Version 5" is recorded; 5.0 vs 5.1.x unknown. The firmware
  CHANGELOG says 5.1 changed Freq jack slew and Q-lock loading, and 5.1.1 fixed
  Env Out 3 and 4. The port follows 5.1.1.
- Freq as MIDI note, plain pitch parameter, or both. **Still yours to decide** (Milestone 4).

## Milestone 1: manual versus source, and other findings for you to decide

Each item says what the manual states, what the source does, and where.

**Decided by Bryn, 2026-10-25: for every item M1-a to M1-t, follow the source.** M1-m (output
labelling): follow the source mapping for now; Bryn will choose the left/right assignment later.
Items that need a capture to check (M1-i, M1-j, M1-k, M1-l, M1-n) stay open for Milestone 3.
Q1 to Q5 (licensing), v5 feature scope and Freq as MIDI or parameter are still undecided.

- **M1-a Freq jack on locked channels.** Manual (Lock section, "impervious to ... the Freq Nudge knob or Freq jack"): a locked channel ignores the Freq Nudge
  knob and the Freq jack. Source: `params.c` `param_read_freq` applies `freq_shift[n]` (the Freq
  jack) outside the `if (!lock[n])` blocks, so locked channels still follow the jack. Only the
  nudge is blocked. Port follows the source unless you decide otherwise.
- **M1-b Scale CV step.** Manual: about 0.45 V per scale. Source: `process_scaleCV` uses 409 ADC
  counts per scale. If 4095 counts is 5 V that is about 0.5 V (**derived**). Scale 11 needs at least
  4090 counts. Jack scaling needs the schematic or a capture.
- **M1-c Transposition clear time.** Manual: hold all six Locks 2 seconds. Firmware CHANGELOG: 3
  seconds. Source: `params.c` compares `lock_down[]` to 36000 main-loop counts (comment says
  about 3 s); the loop has no timer, so real time is unknown.
- **M1-d Fine tune.** Manual: exponential, about 120 cents. Source: `f_nudge = 1 + t/55000`
  (`params.c`), linear in ratio, maximum about 1.0745, about 124 cents (**derived**).
- **M1-e Morph minimum time.** Manual: "a few microseconds". Source: `update_morph` (`rotation.c`)
  gives 16 samples, about 167 us at 96 kHz. Maximum about 1.02 s, which matches the manual.
- **M1-f Spread numbering.** Manual text counts 0 to 16; source `read_spread` gives 1 to 16.
- **M1-g Firmware version.** The manual is v5.0 (Feb 2017) and does not describe the 5.1 changes
  (Freq jack slew, Q-lock loaded from slots). Your unit is "Version 5", 5.0 or 5.1.x unknown. The
  port follows 5.1.1. If your unit is 5.0, behaviour may differ. A capture would show which.
- **M1-h Forbidden notes storage.** Manual: saved only in settings slots (matches `freqblock` in
  the param banks, `system_mode.c`). CHANGELOG: saved in flash and recalled on power-up.
- **M1-i Env Out level, PRE versus POST.** `envout_pwm.c`: PRE uses `>> 18`, POST uses `>> 16` on
  `envelope*channel_level`. That is a 4x difference and POST can saturate. Manual gives no levels.
  Needs a hardware capture.
- **M1-j Timer comments versus code.** TIM4 Period 32000 in code, 16000 in its comment. Trigger hold
  of 285 ticks is commented as about 100 ms, but at the commented 5.6 kHz it is about 51 ms. Real
  rates need a capture or the clock tree worked out from `system_stm32f4xx.c`.
- **M1-k Q CV update rule.** `param_read_q` only accepts a new Q CV value after a change of more
  than 15 ADC counts. Behaviour depends on real ADC noise. Needs a capture.
- **M1-l Codec and analogue path.** The WM8731 ADC high-pass, DAC de-emphasis (`DEEMPH_48k`) and
  -6 dB line-in gain (`codec.c`, `w8731_init_data`) are not DSP code. Corners at 96 kHz are
  unknown. Decide whether Phase 1 emulates them. Needs datasheet and capture.
- **M1-m Output labelling.** Channels 1,3,5 leave by the EVENS OUT jack. Decide how the plugin's
  left and right outputs map to these.
- **M1-n Input level and noise source.** Plugin 0 dBFS versus module volts is undecided. The clip LED
  level (`INPUT_LED_CLIP_LEVEL` 0x58000000) and the manual's 9.6 V imply about 13.96 V full scale
  (**derived**). The noise source is the STM32 DAC noise generator (`dac.c`); its spectrum needs a
  capture or the ST reference manual.
- **M1-o BpRe with user bank.** `filter.c` has no bank 19 branch for BpRe, so `c_hiq`/`c_loq`
  keep stale values. BpRe is reachable only from System Mode.
- **M1-p History clear bug.** `filter.c` bank-change loop zeroes only part of `buf[]` and none of
  `buf_a`. A faithful port reproduces it.
- **M1-q Green banks 1 and 2.** `filter.c` maps bank 13 to alpha_spread2 and 14 to alpha_spread1.
  The manual calls bank 1 "selected notes" and bank 2 "more selected notes". Cannot tell from names
  which is which. Compare note lists against the manual if it matters.
- **M1-r Default scale.** Manual (light ring section): channel 1 is 55 Hz, channel 2 82.5 Hz,
  channel 3 110 Hz, "the default scale when first run". Source defaults (`set_default_param_values`):
  bank 0, scale index 6, notes 5..10, which give 65.40, 73.41, 82.40, 97.99, 109.99, 130.80 Hz from
  `filter_maxq_coefs_Major`. They disagree. The port follows the source.
- **M1-s BpRe input select.** One-Pass BpRe picks the input with `filter_num & 1` rather than
  `channel_num & 1` (`filter.c`). Kept as is.
- **M1-t Startup values.** `freq_nudge` is initialised `{1.0,1.0}` for 2 of 6 elements and `freq_shift`
  defaults to 0 in `set_default_param_values`. What the first audio block sees depends on when the
  first TIM10 tick runs `param_read_freq`. **Corrected in Milestone 2:** the earlier text said the
  main loop runs `param_read_freq`; it does not. Its only caller is `TIM1_UP_TIM10_IRQHandler`
  (`params.c`). See M2-d.

### Other points for your decision
- Single-precision handling and tolerances (`-Ofast`, `-fsingle-precision-constant`): bit-exact is
  not achievable by inspection. Tolerances are agreed in Milestone 3.
- Float to int32 overflow at the accumulation step in `process_audio_block` is undefined in C.
  Hardware behaviour needs a capture.
- Sample rate: firmware runs at 96 kHz fixed. Plugin rate handling is a Milestone 2 design item.
- `limiter.c` is never called; there is no limiter in v5.1.1. Nothing to port.

## Milestone 0 decisions I made that you may want to change

- AU type is a plain **effect** (`aufx`), no MIDI input. The Beads plugin uses a
  MIDI-controlled-effect type to get MIDI. Because Freq-from-MIDI is undecided,
  the type may change at Milestone 4, which changes how Logic lists the plugin.
- Plugin codes: manufacturer `Ptdt` (same as Beads), plugin `Smru`.
- Mono-to-mono and stereo-to-stereo only in Milestone 0.
- Deployment target macOS 12.0 (Beads uses 11.0).
- CI runner `macos-15`, not Beads' `macos-14`: GitHub retires `macos-14` on
  2 November 2026 and fails jobs in scheduled brownouts before then, including
  26 Oct 14:00-24:00 UTC and 29 and 30 Oct (github.blog changelog 2026-10-01).

## Milestone 2 design: decisions

**Decided by Bryn, 2026-10-25: approve the proposed default for every item M2-a to M2-g.**
Full reasoning is in `docs/architecture.md` sections 11 to 14.

- **M2-a Where the scheduler and sample-rate code live.** Section 10 of the audit gave both to
  `src/plugin`. Proposed: tick scheduler in `src/core` (it is firmware timing), resampler and block
  adapter in a new plain-C++ folder `src/host`, so the plugin and the offline renderer share them.
  Alternative: keep both in `src/plugin`.
- **M2-b Audio rate: 96,000 Hz or 95,982.14 Hz.** From `system_stm32f4xx.c` (`PLLI2S_N` 344,
  `PLLI2S_R` 2) and `I2S_Init` in `stm32f4xx_spi.c`, the hardware I2S rate computes to 95,982.14 Hz,
  0.019 % (0.32 cent) below the 96,000 the tables assume. Derived, not measured; the 256 x divider
  formula is from ST's manual, not read here. Proposed default: core at the nominal 96,000, with
  the rate as one constant so it can change later. Needs a capture to confirm what the unit does.
  **Result (CI run 40457983850):** the port rendered at 95,982.14 Hz matches the hardware's evens jack to -0.06 cent mean (+0.26 at 96,000 Hz). Consistent with 95,982.14; the constant in `src/core/Constants.h` is NOT changed yet (decision for Bryn). Assumes the Scarlett's clock is exactly 96,000 Hz.
  **Decided by Bryn (2026-10-26): use 95,982.14 Hz.** Done in `src/core/Constants.h` (`kCoreRateNum` 671875, `kCoreRateDen` 7). CI run 40461296803/40461296936 (commit 9110ee9) green. The host resampler ratios change with it (`ratioToCore`, largest `up` 26875; 44100 Hz is 26875/12348). The 96 kHz host path is no longer bit-exact against the file; `smr_render --core-rate 96000/1` or `--core-rate` with a 96 kHz input uses the one-to-one adapter. Default (resampled) path at 96 kHz, same capture: odds -0.01 to +0.30 cent with trackoffset 15, evens -0.16 to +0.05 cent; 18 of 18 peaks inside 1 cent (run 40461296803, `offset_default_path_resampled`). Not yet measured on the Macs: plugin prepare time and memory with the larger resampler tables (`ratioToCore` comment, 3.4 MB for 44100 Hz).
- **M2-c Main-loop rate.** The source gives none. Proposed placeholder 12,000 iterations per second,
  taken from one comment (`params.c`, `lock_down[] > 36000` is "~3s"). It only affects the Scale CV
  smoothing (`process_scaleCV`, 0.99 per iteration), `read_spread` and `process_rotateCV`. Needs a
  capture (Scale CV step response) to replace it.
- **M2-d Start-up order.** Proposed: `reset()` runs one TIM10 tick and one main-loop iteration before
  block 0, so no block sees `freq_shift` at 0 (the firmware's first TIM10 tick very likely precedes
  its first block). Alternative: reproduce a first block with `c1` = 0.
- **M2-e Tick coalescing and jitter are not modelled.** The audio block occupies about 78 to 104 us
  of each 166.7 us, and TIM10 fires every 48.8 us, so some TIM10 updates merge. Effect: the Freq CV
  moving average and the Env Out follower times are nominal, not measured. Needs a capture.
- **M2-f Input unpack quirk (not ported).** `audio_util.c` `audio_convert_2x16_to_stereo24` adds the
  low 16-bit half as a signed `int16_t`, so whenever that half's top bit is set the input sample is
  65536 too low (up to 2^-15 of full scale, about -90 dB; derived). Whether the codec data makes this
  happen depends on the I2S word layout. Proposed: do not port; the core takes already-unpacked
  int32. Alternative: port it behind a switch for the Milestone 3 comparison.
- **M2-g Verification scope.** V1 (the port compared bit for bit with the unmodified firmware source
  on an Ubuntu runner) is extra work beyond the kickoff pack. Proposed: do it. It is the only check
  that the port matches the source.

## Found while porting (Milestone 2)

- **P-5 Firmware reads past `twopass_calibration[3380]`.** `params.c` `param_read_q` smooths with
  `poll_ctr * (goal - prev) / 15.0` and `poll_ctr` reaches 16, so `qval` overshoots the goal by 1/15 of a
  jump. `filter.c` (two-pass) then uses `twopass_calibration[(uint32_t)(qval_b - 900)]` with
  `qval_b = 1000 + (qc - 3900) * 15`. Any `qval` above about 4118 indexes past the array. Seen in the oracle
  run as qval 4196 and 4203 after a Q lock was released from about 3700 towards 4075. What the hardware reads
  there is not in the source. The port reads the last entry. Needs a capture if it matters.
- **P-6 (M2-d as built).** `reset()` runs one main-loop pass, then one TIM10 tick, before block 0. The approved
  text (M2-d) lists the TIM10 tick first; the code does the loop pass first. Whether that order matters has not been checked.
- **P-9 Switching filter type or mode is covered by V1 for only 588 blocks** (scenario 4), because that scenario
  saturates the output afterwards. Not a known mismatch; a shorter comparison.

## Milestone 4 (parameter layer and audio wiring)

Decisions Bryn approved: D1 wire the core into the plugin audio path in this version (changed from the proposed default); D2 Freq as pitch parameters in octaves, AU type stays `aufx`, MIDI deferred; D3 follow the source for the Freq Nudge power-up sleep; D4 add Filter Mode and Filter Type parameters (41 in total); D5 level-type parameters shown as percent of the ADC range, Freq in octaves.

- **M4-a Parameter to ADC mapping.** Each parameter becomes a 12-bit ADC count (`unitToCounts`, 0..4095) and goes to the ADC index listed in the Controls struct. Relative controls keep the firmware rule: Scale CV is `adc/409` through the 0.99 LPF (`params.c` `process_scaleCV`), Rotate CV is `adc/205` with the +-100 count threshold (`process_rotateCV`). Displayed values for those two are the resulting steps, not volts. Not measured on hardware.
- **M4-b Saved state.** Saved = all host parameters plus one `SMR_ENGINE` block (`Snapshot`, version 1) holding what the hardware keeps in `store_params_into_sram` (notes, scale, bank, locks, Q lock, Q, nudge, filter type/mode, freqblock). Lock parameters win over the saved lock flags after load. Order on load: controls, `Engine::reset()`, snapshot, initial apply, so the Freq Nudge sleep window (`sleep_range` 800, `first_run`) is centred on the loaded knob positions. A nudge saved high stays silent until moved more than 800 counts. Unconfirmed on hardware.
- **M4-c Freq parameter.** Calibrated so the ratio is 2^octaves (trackcomp 1, trackoffset 0). Range about 9.8 octaves (table `exp_1voct[]`), the manual says 10. Resolution about 2.9 cents per count; worst measured end-to-end error 7.22 cents (headless test). The jack only raises pitch. Manual and source disagree slightly on range: not resolved.
- **M4-d Envelope speed.** Fast / Trigger / Slow, default Slow. The M2 renderer defaulted to Trigger by omission. No audio effect found; Env Out is not exposed yet.
- **M4-e Denormal flush.** `juce::ScopedNoDenormals` in `processBlock` is a plugin-only deviation (no firmware equivalent). One line to remove; revisit in M5.
- **M4-f Automation timing.** Host automation is applied once per host buffer, not sample accurate.
- **M4-g Tail length.** 10 s reported to the host. Plugin choice, not from the firmware.
- **M4-h Q CV.** A steady Q CV value moves Q by roughly one 5% step only, because the LPF steps only when the reading changes by more than 15 counts (see M1-k). Firmware behaviour, ported as is.
- **M4-i Mono output.** Mono output sums both output slots (manual: with only EVENS OUT patched all six channels are heard). Mono input is copied to both input slots. Output slot mapping is still the provisional M1-m.
- **M4-j Level CV default.** Level CV parameters default to 100% (assumes the jack is normalled high, which the manual does not state). M3 to confirm.
- **M4-k Trigger parameters.** Rotate CW/CCW, Scale Up/Down act on the rising edge (off to on). In the generic editor, switch on, then off again. Not applied on load.
- **M4-l Not in this milestone.** Env Out outputs, lock hold gestures, Q lock, coarse tuning, lock jacks, 1V/oct env mode, forbidden-notes UI (M7); user-scale editing is not ported.

## Milestone 3 (hardware capture 1): findings for you to decide

Numbers are from CI run 40452093561 (commit 9e18c5d); method in tools/analysis/README.md.

- **M3-a Output mapping, hardware against manual and source.** Manual ("Controls and Jacks") and source (`filter.c`, docs/architecture.md 3.1): channels 1,3,5 leave by the EVENS OUT jack. Measured: with only slider 1 up the signal is on the ODDS OUT jack (-26.4 dBFS rms against -92.4 on the evens jack); with only slider 2 up it is on the EVENS OUT jack (-28.8 against -92.8). Assumes SMR OUT odds to Scarlett input 1 and OUT evens to input 2, and Logic tracks on those inputs (Bryn confirmed both). Not yet explained: the hardware may differ from the manual and source, or slider numbering may differ from channel numbering. The port still follows the source (M1-m). **Decision needed**, and a one-line check by Bryn (is the slider that was up the leftmost one?) would help.
- **M3-b The impulses in `smr_sig_main.wav` are unusable.** Their rings are below the SMR's own noise floor (about -60 dBFS at Q max). The analysis uses the steady noise instead (H = output / input). Nothing to decide; a future capture could use a louder or longer excitation if impulses are wanted.
- **M3-c Q min has no distinct per-channel peaks.** The six Q min bands overlap. Comparing the port at Q min must use the whole response, not per-resonance numbers.
- **M3-d Tolerance consequence.** The noise steps cannot check a 0.5 dB gain tolerance: each step is uncertain by 0.08 to 1.8 dB (1 sigma). Proposal: apply the 0.5 dB gain tolerance to the peak gain and shape of H(f) (P3b, P3c, P4, and the whole Q min response), where the same input drives hardware and port. The 1 cent and 10% tolerances stay as agreed until the comparison shows whether they are realistic. **To be agreed.**
- **M3-e P5 (loopback) not recorded.** The absolute level (M1-n, plugin 0 dBFS against module volts) stays unknown, and peak gains include the Scarlett's own gain. P5 takes about 5 minutes and can be added later as capture-2.
- **M3-f The hardware scale (partly resolved, CI run 40454798347).** Tables searched: `filter_maxq_coefs_*` in `filter_coefs/`, bank map in `filter.c` `process_audio_block`. The measured Q max frequencies match bank 6 (all six channels) at notes 6 to 11 of scales 2 (odds), 10 (channel 2) and 2 (channels 4, 6), constrained by the P4 rotation result. Not determined: the odds scale (table values are equal in every scale at those notes), and bank 6 versus bank 2 for channels 4 and 6 (differ by at most 0.15 cent). The unit has different scales on different channels. The port's evens frequencies at this state are +0.16 to +0.35 cent from the hardware.
- **M3-g The odds jack is 5.0 to 6.1 cent below the hardware when the port is rendered at the identified state.** Evens has no such offset. **Answered by Bryn:** the odds Nudge knob was fully counter-clockwise. **CI result (run 40457983850, hardware rate 95,982.14 Hz):** the odds offset is exactly two steps of `exp_1voct` (+5.80 cent): odds trackoffset 14 gives -2.89 cent, 15 gives +0.01, 16 gives +2.89 (port minus hardware, odds mean). Evens needs no offset. What is not known: whether the two steps are the unpatched odds Freq-jack ADC reading higher than the port's assumed 0, or a stored `trackoffset[0]` (loaded from flash, `system_mode.c`; set by the calibration in `user_scales.c`). The audio cannot tell them apart. The Nudge explanation would need about 185 ADC counts at the knob's minimum and is not what the source predicts for a fully counter-clockwise knob, but the knob's ADC floor was not measured. **Decided by Bryn (2026-10-26):** keep the port's default `trackoffset` 0 (source default, `params.c set_default_param_values`); 15 is used only in comparisons. Confirmed again in run 40461296803 at the 671875/7 core rate: odds trackoffset 15 is +0.01 cent, 14 is -2.89, 16 is +2.89.
- **M3-h Q mid knob position.** **Answered by Bryn:** 12 o'clock by best estimate (50%). **CI result:** the port's width is very sensitive just below noon, because `filter.c` makes the first filter reach its maximum Q at noon (`qval_a = qc * 2`, capped at 4095). The hardware P3b widths (1.134 to 1.312 Hz) are reproduced by one Q count of 1876 for all six peaks (45.8% of travel; 172 counts below noon): widths within -8.8 to +5.8%, 6 of 6 inside 10%. So the P3b width miss is a knob position within the error of "dead centre", not a port difference. The count includes any Q CV contribution (`param_read_q`); the port assumes 0. The per-peak counts range 1832 to 1901 because the hardware odds widths grow with frequency (1.134, 1.296, 1.312 Hz) and the port's do not. Nothing in the port is changed.
- **M3-i Higher gain in the port at high frequencies.** **CI result (run 40457983850):** the port-minus-hardware gain at Q min follows a first-order shelf with corners at 6366 Hz and 21221 Hz (the 50 us / 15 us de-emphasis pair doubled) to 0.09 dB rms (odds) and 0.07 dB rms (evens); the same shelf at the 48 kHz corners (3183, 10610 Hz) fits to 1.2 dB rms; a straight line to 1.3 dB rms. The P4 peaks above 5 kHz agree within 0.24 dB (odds) and 0.82 dB (evens). This points to the codec, not the firmware's DSP: `codec.c w8731_init_data` Reg 05 sets `DEEMPH_48k` and Reg 08 says "48k ADC/DAC" while the firmware runs at 96 kHz (see M1-l). Not verified: the WM8731 datasheet text I could read does not give the de-emphasis time constants (my 50 us / 15 us is a recollection), nor whether the codec's internal rate is 96 kHz. P5 (loopback) would separate the Scarlett's own response, but a smooth shape identical on both jacks fits the codec better than the interface. **Decision needed:** whether Phase 1 emulates the codec's de-emphasis (a -6.7 dB tilt at 16 kHz in the hardware's sound that the firmware code does not contain). Nothing is changed in the port. **Bryn's answer (2026-10-26): yes, if I recommend. My recommendation: later, not in Phase 1 core.** Reasons: the cause is not verified (P5 loopback not recorded, so the Scarlett's own response is not separated); the 50/15 us constants are my recollection, not read from the datasheet; the firmware code does not contain it, and `src/core` is the firmware port. If P5 confirms the codec, implement the measured shape as an optional output filter outside `src/core`, default on or off as Bryn decides then.
- **M3-j Tolerance review (proposal).** Update after run 40457983850: frequency, 1 cent: 18 of 18 inside at the hardware rate with the odds offset. Width, 10%: 7 of 18 peaks inside at the nominal Q counts 2048 and 4095 (offset comparison, run 40457983850); 6 of 6 P3b peaks inside at Q count 1876. Gain, 0.5 dB: 4 of 18 at the nominal counts; the misses come from the high-frequency tilt (M3-i) and Q max (M3-k). Tolerances not loosened. **To be agreed.**
- **M3-k Q max widths (new).** At Q count 4095 the port's widths are 0.580 to 0.616 Hz. Hardware: 0.515, 0.554, 0.623 Hz (odds, 219, 437, 875 Hz) and 0.521, 0.457, 0.616 Hz (evens, 409, 523, 1046 Hz). Four of six are narrower than the narrowest port width at any Q count in the sweep (`param_read_q` caps `qval_goal` at 4095, so a higher count is not possible). Not a knob position. Causes not tested. Candidates to test next, all unverified: the measurement of very narrow peaks on a noisy hardware recording (add hardware-level noise to the port render and re-measure); per-channel Q on the hardware (Q lock, `q_locked[]`); firmware 5.0 versus 5.1.x (M1-g).
  **Findings (CI run 40461296803, commit 9110ee9, job port-noise; `tools/analysis/port_noise.py`):** (a) Q-lock is ruled out by source: `param_read_q` in `params.c` caps `qval_goal` at 4095 on both the locked and unlocked paths. (b) Firmware 5.0 vs 5.1.x is ruled out for the filter and Q path: `git diff` between the pinned commit and the earlier proxy commit a600ec6 shows `filter.c` differs only by an `#include` and the tables are unchanged. (c) Rounding is ruled out: a build with fused multiply-add changes the output by -102 to -107 dB rms and the widths by under 0.002 %. (d) The width estimate is noise-limited on this recording: the same hardware peaks measured on two stretches (2-40 s and 55-105 s) differ by -17 % to -87 % (P3c odds S2 vs S1: -31.5, -77.0, -87.1 %), the noise-free port differs by under 7 %. (e) The hardware's noise floor is resonator-shaped: at Q max 100 % of its power lies within +-3 Hz of the three resonances (floor -58.6 dBFS odds, -61.1 evens), so output-side white noise (model A) changes widths by at most 0.27 %; input-side noise (model B, white noise into the filter) is the relevant model. (f) Model B at the level that matches the hardware floor, 8 seeds: P3c (Q max) rms z over the six peaks is 1.17 at -3 dB and 1.23 at 0 dB (noise-free port: widths +15.8, +8.6, -6.8 / +13.9, +34.9, -1.6 % off), so noise at the measured floor level brings the port's Q max widths into the hardware range. P3b (Q count 1876) does not improve with noise: smallest rms z 2.60 at -3 dB, with the odds peaks at z +3.8 to +5.9; the noise-free port is already within -9.7 to +4.6 %. **Not confirmed as the cause:** model B (white noise at the input) is my assumption about where the hardware noise enters, the z-scores use the port's own seed spread, and P3b is not matched equally well. Consequence: the 1876 Q count fit (M3-h) may be biased by noise, and width tolerance of 10 % cannot be judged on this recording at Q max. A better test needs a cleaner capture (average of several takes, or a shorter, louder excitation).

## Milestone 5 (plugin DSP match)

Numbers are from CI runs on commit 94d3d31 (build run 40467599259, analysis run 40467599121, logic-compare workflow on later commits); details in docs/status.md.

- **M5-a Plugin against core.** `tests/plugin_tests.cpp` (`smr_plugin_tests selftest`) drives the real `SmrAudioProcessor` headless and compares it with `BlockAdapter` plus `ParamApplier` driven directly. 33 checks pass on arm64 and on x86_64 (Rosetta): bit-identical output at 44100, 48000 and 96000 Hz, host blocks 64, 333, 512 and 1000, Q min, mid and max, a script of 16 parameter changes (levels, spread, morph, bank, lock, rotate, scale, nudge, Freq, slew, switches, filter mode and type), mono in/mono out, mono in/stereo out, stereo in with L equal R, saved state loaded (capture-1 snapshot) and copied between instances. The plugin output does not depend on the host block size. Fidelity against the hardware is unchanged by this: the plugin is the same as the core, and the core's differences from the hardware (M3-g, M3-i, M3-k) are still open.
- **M5-b Latency.** `getLatencySamples()` reports `ceil(latencyHostFrames())`: 26, 29, 33, 35, 48, 51, 77, 83 samples at 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000 Hz. The first output sample above 1e-9 after an impulse comes at index 19, 25, 35, 37, 68, 75, 138, 150. That is not a latency measurement (a resonator's response has no sharp start; the two numbers differ in both directions), so the reported value has not been checked independently. `smr_render` trims `llround(latencyHostFrames())`, the plugin renderer trims the reported value: the renders differ by a one-sample shift and are identical after it. Logic's own bounce alignment is tested in docs/testing-milestone-5.md.
- **M5-c Odds Freq offset in the plugin.** The plugin has no `trackoffset`. The Freq (odds) parameter at 0.009668 octaves is ADC count 15 (`octavesToCounts`), which gives table index 4, the same index as `trackoffset[0] = 15` with the jack at 0. Through the plugin (JUCE parameters, saved-state snapshot, rotate triggers) the capture-1 passes are bit-identical to `smr_render --trackoffset 0=15` (all 11,039,999 samples per file, after the one-sample shift, run 40467599121). Note: Freq 0 octaves is ADC count 11, not 0 (index 0 of `exp_1voct[]`, M4-c); the parameter cannot go lower. Decision from earlier stands: the plugin default is `trackoffset` 0 and no offset.
- **M5-d The hardware's identified state cannot be set from the plugin window.** Capture-1 needs scales 2/10/2/2/2/2 on the six channels (M3-f). The window's controls change bank and scale for all unlocked channels together (Bank, Scale up/down, Rotate), so that state is reachable only through saved state. CI therefore renders the plugin at that state through `setStateInformation`. A Logic recording at the capture-1 state is not possible. Options: (1) accept CI at the capture-1 state plus the Logic bounce comparison at defaults as the plugin-to-hardware chain; (2) a capture-2 on the hardware at the source's default state (bank 0, scale 6), which also lets P5 (loopback, M3-e) be recorded and so separates the Scarlett from the SMR. **Decision needed.**
- **M5-e Denormals.** `ScopedNoDenormals` stays (M4-e). Test: 100 s at Q max at 48 kHz (1 s noise, 99 s silence), plugin (flush on) against reference (flush off): 0 of 4,800,000 samples differ on each channel, the output reaches exactly 0, and the processing time is the same (plugin 1.24 to 1.95 s, reference 1.24 to 1.92 s for 100 s of audio; CI runner timing, not a CPU measurement). So in this scenario denormals did not matter; the flush has no measured effect and no measured benefit. Whether to keep it is yours to decide; I recommend keeping it, because it removes a possible CPU spike in other hosts and changes no sample in this test.
- **M5-f Host rates.** The plugin runs at every integer host rate whose ratio to the core rate reduces to integers up to 30000 (`BlockAdapter::prepare`), for example 50000 Hz (215/112) as well as the eight common rates. Rates outside that, for example 44101 Hz, pass the audio through unchanged with latency 0 (tested). The `smr_render --help` text still lists the eight common rates as "the exact-ratio list"; the code accepts more.

