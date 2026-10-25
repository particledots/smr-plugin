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
  text said the other order; the main loop has to run first because `freq_shift` is computed from values it sets.
- **P-9 Switching filter type or mode is covered by V1 for only 588 blocks** (scenario 4), because that scenario
  saturates the output afterwards. Not a known mismatch; a shorter comparison.
