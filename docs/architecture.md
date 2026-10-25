# Architecture audit (Milestone 1)

Source: `third_party/SMR` at 79bc953bb4b9cf47614d317b4ba9fc9f615c43ea (v5.1.1).
Manual: "Eurorack Module User Manual v1.1.1 (firmware v5: February 2017)", plus "Controls and Jacks".
Method: read the C files named below; every claim gives a file and function. Where a claim is
derived (not stated in source) it is marked **derived**. Where only hardware can settle it it is
marked **needs capture**. No code was changed in this milestone. Nothing here has been run.

Manual-versus-source disagreements are in `docs/open-questions.md` (items M1-a to M1-t).

## 1. Constants and block structure

`globals.h`: NUM_FILTS 20, NUM_CHANNELS 6, NUMSCALES 11, NUMSCALEBANKS 20, SAMPLERATE 96000,
codec_BUFF_LEN 128. `FW_VERSION 0x05`. `enum Filter_Modes {TWOPASS=2, ONEPASS=3}`,
`enum Filter_Types {BPRE, MAXQ}`.

- The audio callback is `process_audio_block(int16_t *src, int16_t *dst, uint16_t ht)` in
  `filter.c`. It is called from `DMA1_Stream3_IRQHandler` in `i2s.c`.
- Block size is MONO_BUFSZ = 16 frames (comment in `filter.c`, derived from the macros), so
  96000/16 = 6000 blocks per second (**derived**).
- Input arrives as packed int16 pairs and is unpacked by `audio_convert_2x16_to_stereo24`
  (`audio_util.c`); output is packed by `audio_convert_stereo24_to_2x16`.
- Filter state per channel, scale and filter: `buf[NUM_CHANNELS][NUMSCALES][NUM_FILTS][3]`, and
  `buf_a[...]` for the first pass (`filter.c`). All six channels run all 11 scales x 20 filters
  at once so that morphing between scales needs no state reset.

## 2. Timing domains

| Domain | Where | What runs | Rate |
|---|---|---|---|
| Audio DMA ISR | `i2s.c` `DMA1_Stream3_IRQHandler`, priority 0/0 | `process_audio_block` | 6 kHz blocks (derived) |
| TIM10 ISR | `params.c` `TIM1_UP_TIM10_IRQHandler`, priority 1/0 | `param_read_freq`, `update_slider_LEDs`, `update_motion` | about 20.5 kHz per code comment, 168 MHz/(8195+1) |
| TIM9 ISR | `envout_pwm.c` | `update_ENVOUT_PWM` | about 5.6 kHz per comment |
| TIM4 ISR | `dig_inouts.c`, priority 3/2 | debounce Rotate and Lock trigger jacks | see below |
| Main loop | `main.c` | everything else (see section 4.2) | unthrottled |

Facts that affect the port:
- The audio ISR has the highest priority, so one block is atomic with respect to the other ISRs.
  Parameters can change between blocks, never inside one (`main.c`, `i2s.c`).
- Per-sample work inside the block: `update_morph()` (`rotation.c`).
- Per-block work inside the block: `param_read_q()` (`params.c`).
- The main loop has no timer. Counters such as `lock_down`, `rotary_button_down` hold times and
  the `SCALECV_LPF` 0.99 filter in `process_scaleCV` count loop iterations, so their real-time
  durations depend on how fast the loop runs (**needs capture**).
- Timer comments and timer code disagree in places (TIM4 Period is 32000 in code but 16000 in its
  comment; the "every 20us" comment in `update_ENVOUT_PWM` does not match the 5.6 kHz period).
  The rates above are the code comments' numbers, not measured (M1-j).
- SystemCoreClock is 168 MHz (`system_stm32f4xx.c`), PPRE2_DIV2, PPRE1_DIV4.

## 3. Signal path (what `process_audio_block` does)

For each 16-frame block (`filter.c`):

1. Unpack input to int32 (`audio_convert_2x16_to_stereo24`). Clip LEDs set at
   `INPUT_LED_CLIP_LEVEL` 0x58000000 (`audio_util.c`).
2. `param_read_q()` once; `update_morph()` per sample.
3. For each channel (0..5) and each of the 20 filters: choose scale (current scale, plus the
   morph target scale `motion_fadeto_scale` and note `motion_fadeto_note`, crossfaded by
   `motion_morphpos`), look up coefficients, run the filter, write `filter_out[filter][i]`.
4. Blend filters into the channel (`f_blended`), apply `channel_level` (slider and level CV, with
   per-sample ramps), accumulate into `filtered_buffer` (channels 2,4,6) or `filtered_bufferR`
   (channels 1,3,5).
5. Float to int32 accumulation: `filtered_buffer[i] += f_blended*channel_level[j]`. Out-of-range
   float to int conversion is undefined in ISO C. The port must clamp explicitly (see 8).
6. Pack to int16 pairs (`audio_convert_stereo24_to_2x16`).
7. Env Out: `ENVOUT_preload` set from the block (`filter.c`), consumed by `update_ENVOUT_PWM`.

### 3.1 Input and output mapping
- Source (`filter.c`): the first input slot (`left_buffer`) feeds channels 1,3,5 (the `channel_num`
  even test). Channels 1,3,5 (`j` even) are summed into `filtered_bufferR`, the second output slot;
  channels 2,4,6 go to `filtered_buffer`, the first output slot.
- Manual and "Controls and Jacks": odds IN feeds channels 1,3,5 and channels 1,3,5 leave by the
  EVENS OUT jack (an easy trap). Evens IN is normalled to odds IN, and odds OUT to evens OUT. That
  normalling is jack switches in the analogue hardware; there is no firmware code for it.
- One-Pass BpRe exception: it picks its input with `filter_num & 1`, not `channel_num & 1`
  (`filter.c`). Kept as is in the port (M1-s).
- Plugin consequence: a mono input must be copied to both input slots. Which plugin channel is
  "odds out" and "evens out" is your decision (open question M1-m).

### 3.2 Filter modes
Constants shared: coefficient c1 comes from `table[scale*21 + note] * freq_nudge * freq_shift`,
clamped to 1.30899581 (about 20 kHz) (`filter.c`). Tables are 21 floats per scale (20 notes plus one
pad), 11 scales, so 231 floats for MAXQ (`filter_coefs/*.h`: `filter_maxq_coefs_*`, parsed: 231
values). BpRe tables are 693 floats (21 x 3 x 11): `filter_bpre_coefs_*_800Q` and `_2Q`.
The header declares the MAXQ array as `[231][3]` but supplies 231 initialisers, so only the first
77 rows of three are filled and the rest is zero. It is indexed flat through a `float *` cast in
`filter.c`, so this is harmless. The port should declare `float[231]`.

**Two-Pass MAXQ (default: `filter_mode=TWOPASS`, `filter_type=MAXQ`)** (`filter.c`)
- `qval_a = min(qc*2, 4095)`; `qval_b = 1000` if `qc < 3900`, else `1000 + (qc-3900)*15`.
- `c0 = 1 - exp_4096[(uint32_t)(qval/1.4)+200]/10`; `c2 = 0.003*c1 - 0.1*c0 + 0.102`.
- Crossfade between pass A and pass B from CF_MIN 1830 to CF_MAX 3630 (CROSSFADE_POINT
  4095*2/3, CROSSFADE_WIDTH 1800).
- `ratio_b *= 43801543.68f / twopass_calibration[(uint32_t)(qval_b-900)]` (`filter.c`, line 415).
- Output: `filter_out = ratio_a*filter_out_a - filter_out_b`.

**One-Pass MAXQ and One-Pass BpRe** (`filter.c`). BpRe is reachable only from System Mode
(`system_mode.c`). BpRe uses `c_hiq`/`c_loq` with stride 63, a `log_4096` Q blend and a nudge
interpolation between neighbouring notes. Freq jacks do not track 1V/oct in BpRe. Bank 19 (user
scales) has no BpRe branch, so `c_hiq`/`c_loq` stay stale there (M1-o).

**Bank map** (`filter.c`, scale_bank to table): 0 Major, 1 Minor, 2 western_eq,
3 western_twointerval_eq, 4 twelvetone, 5 diatonic_eq, 6 western, 7 western_twointerval,
8 diatonic_just, 9 indian, 10 shrutis, 11 mesopotamian, 12 gamelan, 13 alpha_spread2,
14 alpha_spread1, 15 gammaspread1, 16 17ET, 17 bohlen_pierce, 18 B296, 19 `user_scalebank`
(MAXQ only). `filter_coef.h` also includes `filter_coef_gamma.h` and `filter_coef_video.h`, which
no code references (grep). The manual's group and bank order (Blue 6, Pink 3, Orange 4, Green 6,
White 1) matches names and order for banks 0..12, 16..19. Banks 13 and 14 (alpha_spread2 then
alpha_spread1) versus manual Green banks 1 and 2 ("selected notes", "more selected notes") cannot
be checked by name (M1-q).

### 3.3 Smoothing and levels
- Q: `param_read_q()` (`params.c`) runs once per block with a poll counter of 16. `qvalcv` updates
  only when the ADC moved more than 15 counts from the last accepted reading, then steps by
  `QCV_LPF` 0.95 (integer truncation). Pot: `QPOT_LPF` 0.95. Q-lock: `q_locked[]`, `qval_goal[]`,
  `QPOT_MIN_CHANGE` 100. `qval` is interpolated over 15 steps between polls. The CV rule depends
  on ADC noise (M1-k).
- Level (the "slew"): every 51 samples per channel, `level = adc_buffer[LEVEL]/4096 *
  (slider-20)/4096`; `level_goal` is smoothed with `CHANNEL_LEVEL_LPF` and linearly interpolated
  over 50 samples. `CHANNEL_LEVEL_LPF = 1 - exp_4096[adc_MORPH/2+137]` when CV Slew is on
  (switch `CVLAG`, latched when flipped), else `CHANNEL_LEVEL_MIN_LPF` 0.75 (`params.c`, `filter.c`).
- Morph: `update_morph()` (`rotation.c`): `f_morph = 0.999*f_morph + 0.001*exp_4096[adc MORPH]/16`;
  `morphpos` advances by `f_morph` per sample and clamps at 1.0.
- Freq jack conditioning: `(adc + trackoffset + BASE_TRACKOFFSET(-11)) * trackcomp`, clamped 0..4095,
  then a 40-tap FIR low-pass (`init_freq_lpf`, `lpf.c` `apply_fir_lpf`), then `apply_bracket` with
  bracket_size 2, then `exp_1voct[]` (4096 entries, 1.0 to 954.088, doubling near index 414)
  (`params.c` `param_read_freq`).
- Fine tune: `f_nudge = 1 + t/55000`, linear in ratio, max about 1.0745 (about 124 cents,
  **derived**). Coarse: 13 zones giving -6..+6 semitones, with `fknob_lock`/`coarse_lock`
  value-crossing logic (`params.c`).

### 3.4 Limiter
`limiter.c` (`init_limiter`, `limiter`) is never called. The only reference is
`#include "limiter.h"` in `main.c` (grep: no call sites). There is no limiter in the v5.1.1 signal
path. The audio path simply saturates at the int32/int16 conversion (see 8).

### 3.5 Env Out (`envout_pwm.c`, `update_ENVOUT_PWM`, `filter.c`)
- `ENVOUT_preload` = absolute value of the first sample of the block of `f_blended` (before the slider).
- Followers: attack/decay 0.9990/0.9991 (Fast), 0.9995/0.9999 (Slow).
- PRE: `ENVOUT_PWM = (uint32)envelope >> 18`. POST: `(uint32)(envelope*channel_level) >> 16`.
  The shifts differ by 4x and POST can saturate (M1-i).
- Trigger mode: threshold 0x02000000, 285 ticks hold (M1-j).
- 1V/oct mode: `ENVOUT_preload = c1` mapped through `FreqCoef_to_PWMval` (ARM `usat` asm, line 503),
  minus 127.96875, scaled by `voltoct_pwm_tracking`.
- Switch: 3 positions (`ENVSPEEDFAST`, `ENVSPEEDSLOW`, else Trigger = centre). Holding ROTATE
  (`ROTARY_SW`) while flipping to Fast selects `ENV_VOLTOCT`.

## 4. Dependencies of the four areas you asked about

### 4.1 `filter.c` (`process_audio_block`)
Globals read: `left_buffer`, `right_buffer`, `note`, `scale`, `scale_bank`, `motion_fadeto_note`,
`motion_fadeto_scale`, `motion_morphpos`, `freq_nudge`, `freq_shift`, `qval`, `channel_level`,
`CHANNEL_LEVEL_LPF`, `potadc_buffer` (sliders), `adc_buffer` (level, morph), `ui_mode`,
`env_track_mode`, `env_prepost_mode`, `filter_type`, `filter_mode`, `user_scalebank`, `c_hiq`,
`c_loq` (declared in `filter.c`).
Globals written: `filtered_buffer`, `filtered_bufferR`, `filter_out`, `buf`, `buf_a`, `c_hiq`,
`c_loq`, `ENVOUT_preload`, `slider_led_mode`.
Calls: `param_read_q()`, `update_morph()`, `audio_convert_*`, `LED_SLIDER_ON` (hardware LED, stub).
Tables: `exp_4096`, `exp_1voct`, `log_4096`, `twopass_calibration`, `filter_coef.h`.
Defect: the history clear on bank change (`for (j=0; j<NUMSCALES*NUM_FILTS; j++){*(ff+j)=0; ...+1; ...+2}`)
zeroes only floats 0..221 of `buf[i]` (660 floats) and never touches `buf_a`, although the comment
says it clears the history (M1-p). A faithful port reproduces this; it only matters at bank change.
Dead code: `equal_pow_pan_padded.h` (`epp_lut`) is included in `filter.c` and never used.

### 4.2 `params.c` (and `main.c` loop order)
Main loop call order (`main.c`): `param_read_switches`, `update_LED_ring`, `update_lock_leds`,
`read_spread`/`update_spread`, `process_lock_jacks`, `process_lock_buttons`, `process_freq_blocks`,
`process_rotary_button`, `process_rotary_rotation`, `check_rotary_pressed_repeatedly`, the
edit-mode handlers, `do_ROTUP`/`do_ROTDOWN`, `process_rotateCV`, `process_scaleCV`.
- Output globals (read by the DSP): `freq_nudge[]`, `freq_shift[]`, `qval[]`, `lock[]`,
  `q_locked[]`, `mod_mode_135`, `mod_mode_246`, `coarse_adj[]`, `freqblock[]`.
- Inputs: `adc_buffer[]`, `potadc_buffer[]` and switch pins (`dig_inouts.h`: ADC index map
  MORPH_ADC 0, QVAL_ADC 1, LEVEL_ADC_BASE 2..7, FREQNUDGE1_ADC 8, FREQNUDGE6_ADC 9, SCALE_ADC 10,
  SPREAD_ADC 11, ROTCV_ADC 12, FREQCV1_ADC 13, FREQCV6_ADC 14; ADC3: QPOT 0, sliders 1..6).
- Spread: `spread = (adc>>8)+1`, 1..16. Rotate CV: `adc/205`, 0..19. Scale CV: `lpf_buf/409`, 0..10
  (`read_spread`, `process_rotateCV`, `process_scaleCV`).
- Freq CV on locked channels: in `param_read_freq` the line `freq_shift[0]=f_shift_odds;` and its
  siblings (lines 521-594, 669-717) sit outside the `if (!lock[n])` blocks, so locked channels still
  follow the Freq jack. The manual says otherwise (M1-a).
- Hardware inside `params.c`: ADC reads, GPIO, timers (`init_freq_update_timer`), TIM10 ISR.

### 4.3 `rotation.c`
- Globals: `motion_*`, `f_morph`, `spread`, `rot_dir`, `scale_bank_defaultscale[]` (defined line 69,
  grep shows no other use: dead).
- Reads: `lock[]`, `freqblock`, `rotate_to_next_scale`, `note[]`, `scale`, `adc_buffer[MORPH_ADC]`,
  `flag_update_LED_ring`.
- Functions: `update_spread`, `update_morph`, `update_motion` (TIM10 tick), `rotate_up/down`,
  `change_scale_up/down`, `jump_scale_with_cv`, `jump_rotate_with_cv`, `is_morphing`, `is_spreading`.
- Header mismatch: `rotation.h` declares `update_spread(uint8_t force_update)`; `rotation.c`
  defines `update_spread(int8_t t_spread)`. Use the `.c` definition.
- LED coupling: `flag_update_LED_ring` is a UI side effect; stub as a no-op.

### 4.4 Limiter and level slew
See 3.4 (no limiter) and 3.3 (level smoothing). `lpf.c` (`o_analog`, `setup_fir_filter`,
`apply_fir_lpf`, `apply_bracket`) is used only for the two Freq-jack conditioners in `params.c`.

## 5. Hardware path that is not in the firmware DSP

These change the sound at the jacks but are not code you can port. They are decisions for Bryn
(M1-l, M1-n) and need datasheet or capture:
- Codec Wolfson WM8731 (`codec.c`, `w8731_init_data`): line-in volume `VOL_n6dB` (-6 dB), ADC
  high-pass on (`ADCHPFEnable`), DAC de-emphasis `DEEMPH_48k`, 24-bit slave. The Reg 08 comment says
  "48k ADC/DAC" while SAMPLERATE is 96000. Corner frequencies at 96 kHz **needs datasheet and capture**.
- Input scale: clip LED at `INPUT_LED_CLIP_LEVEL` 0x58000000; manual says about 9.6 V. Implied
  full scale about 13.96 V is **derived**, unverified.
- Noise source: STM32 DAC channel 1 noise generator (`dac.c` `DAC_Ch1_NoiseConfig`, `TIM6_Config`,
  TIM6 period 0x4000), normalled to odds IN. Spectrum **needs capture or ST reference manual**.
- Analogue front end and output stage, jack normalling.

## 6. Keep, stub or drop

| Keep (becomes src/core) | Stub or replace (src/plugin) | Drop |
|---|---|---|
| `process_audio_block` maths, `filter_out`, `buf`, `buf_a` | ADC reads as host parameters (`adc_buffer`, `potadc_buffer`) | `flash.c`, `mem.c`, `system_mode.c` flash layout (replace by host state) |
| `param_read_q`, `update_morph`, `update_motion`, rotate/scale/spread logic | LED calls (`LED_SLIDER_ON`, `flag_update_LED_ring`, `update_slider_LEDs`): no-ops | `led_ring.c`, `leds.c`, `pca9685_driver.c` |
| `param_read_freq` logic and `lpf.c` | `ENVOUT_PWM` hardware: plugin may expose the envelope value later | `adc.c`, `i2s.c`, `codec.c` (replace by host audio) |
| Coefficient tables, `exp_4096`, `exp_1voct`, `log_4096`, `twopass_calibration` | `user_scales.c` edit UI: keep `user_scalebank` data, defer the edit modes | `rotary.c/.h` (GPLv3, hardware only), `dac.c` hardware parts |
| `globals.h` constants | Calibration values: fixed at trackcomp 1, trackoffset 0, voltoct_pwm_tracking 1.0 | startup, linker, ST peripheral library, `limiter.c`, `equal_pow_pan_padded.h`, unused coefficient files |
| `envout_pwm.c` envelope maths | Noise source: new generator, decision M1-n | ARM `arm_math.h` (not copied, see 7) |

Timer-driven pieces (`update_motion` at TIM10 rate, `update_ENVOUT_PWM` at TIM9 rate) need a defined
tick schedule in the plugin. That is a Milestone 2 decision and should be agreed against the rates
above, which are only code comments.

## 7. Licences (Q6, Q7, Q8)

- Q6: `rotary.c` and `rotary.h` say "Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3."
  Both are encoder decoding and GPIO setup only (`init_rotary`, `read_rotary`), nothing the DSP needs.
  They stay out of `src/core` and out of the plugin.
- Q7: grep finds no `arm_*(` function call in any `.c` or `.h` outside `arm_math.h`. The two
  references are `#include "arm_math.h"` (`filter.c`) and `#include <arm_math.h>` (`audio_util.h`).
  The only ARM-specific code on the DSP path is the `usat` asm in `envout_pwm.c` (line 503), plus
  `nop` asm in `main.c` and `pca9685_driver.c`. `stm32/core/include/arm_math.h` has a header
  "Copyright (C) 2010 ARM Limited. All rights reserved" and no licence text (grep for "licen",
  "permission", "redistribut", "warrant" found nothing). Do not copy it. The port needs no CMSIS.
- Q8: the string "stmlib" appears only in the firmware `README.md`. No source file or Makefile uses
  it. Not needed.
- Everything else audited carries the MIT notice (`filter_coef.h` header shown; `LICENSE` in the repo
  root). The firmware `LICENSE` says the hardware is CC BY-NC-SA; see Q5.

## 8. Numerical fidelity notes

- Makefile flags: `-Ofast -fno-tree-loop-distribute-patterns -mcpu=cortex-m4 -mfloat-abi=hard
  -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion`.
- `-fsingle-precision-constant` makes bare constants such as `0.003` and `1.0` float on the STM32.
  A desktop build would treat them as double unless the port writes explicit `f` literals.
- `-Ofast` allows reordering and fused multiply-add. Bit-exact match with the hardware is therefore
  not achievable by reading the source. Plan: explicit float literals, `-ffp-contract=off` for
  `src/core`, tolerances agreed in Milestone 3 (**not yet agreed**).
- Float to int32 accumulation is undefined when out of range. The port must clamp. Hardware
  behaviour at overload **needs capture**; do not assume saturation.
- The plugin host rate is not 96 kHz. A rate-conversion decision is needed (Milestone 2).
- Table checks (parsed): `exp_4096` 4096 values, 0.999999 to 0.000163, strictly decreasing;
  `exp_1voct` 4096 values, 1.0 to 954.088; `log_4096` 0.25 to 1.0; `twopass_calibration` declared
  3380, 3354 initialisers, indices used 100..3025.

## 9. Persistence

`system_mode.c` keeps 6 parameter banks plus globals in flash (`flash_*` arrays), and
`factory_reset()` / `load_params_from_sram()`. In the plugin this becomes host state. Defaults
(`set_default_param_values`): `note[i]=i+5`, `scale=6`, `scale_bank=0`, `filter_type=MAXQ`,
`filter_mode=TWOPASS`, locks and Q-locks 0, `qval` 0, `freq_nudge` 0, `freqblock` 0.
`freq_nudge` is initialised `{1.0,1.0}` for 2 of 6 elements; the rest are 0. `freq_shift` defaults to 0
there. A faithful port needs to confirm what the first block actually sees (M1-t).

Default notes: bank 0 (Major), scale index 6, notes 5..10 give 65.40, 73.41, 82.40, 97.99, 109.99,
130.80 Hz, computed from `filter_maxq_coefs_Major` (c1 x 96000/2pi). The manual says channel 1 is
55 Hz, channel 2 82.5 Hz, channel 3 110 Hz (M1-r).

## 10. src/core versus src/plugin boundary

`src/core` takes plain structs: a `Params` struct (the values that `params.c` writes today),
`process(const float *in, float *outOdd, float *outEven, int n)` blocks of 16, and a `State`
holding `buf`, `buf_a`, morph and motion state. No JUCE, no globals touched by hardware, no
file I/O. `src/plugin` owns parameters, host state, GUI, sample-rate handling and the tick
scheduler. Hardware-only items from section 6 never enter `src/core`.
