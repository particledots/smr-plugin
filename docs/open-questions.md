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

Each item says what the manual states, what the source does, and where. None are resolved.

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
  main loop first runs `param_read_freq`. Needs a check against start-up behaviour.

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
