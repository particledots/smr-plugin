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
file I/O. `src/plugin` owns parameters, host state and the GUI. Hardware-only items from
section 6 never enter `src/core`.

**Changed in Milestone 2 (needs your approval, see section 14):** the tick scheduler moves into
`src/core` (it is firmware timing, identical for every host), and sample-rate conversion plus the
16-frame block adapter go into a new plain-C++ folder `src/host` (no JUCE), so that the plugin and
the offline renderer share one implementation. This replaces the earlier wording that gave both to
`src/plugin`.

## 11. Tick scheduler (Milestone 2 design, not yet implemented)

### 11.1 What runs when in the firmware
| Context | File, function | Runs | Period register | NVIC priority |
|---|---|---|---|---|
| Audio DMA | `i2s.c` `DMA1_Stream3_IRQHandler` (`codec.h`: `AUDIO_I2S_EXT_DMA_IRQ` is `DMA1_Stream3_IRQn`) | `process_audio_block` on 16 frames, once per half-transfer | I2S clock | 0 / 0 (`i2s.c` `I2S_Block_Init`) |
| TIM10 | `params.c` `TIM1_UP_TIM10_IRQHandler` | `param_read_freq()`, `update_slider_LEDs()`, `update_motion()` | 8195 (`init_freq_update_timer`) | 1 / 0 |
| TIM9 | `envout_pwm.c` `ENV_update_IRQHandler` (`envout_pwm.h:41` defines it as `TIM1_BRK_TIM9_IRQHandler`, so it is wired) | `update_ENVOUT_PWM()` | 30000 (`init_ENV_update_timer`) | 1 / 0 |
| TIM4 | `dig_inouts.c` `debounce_trigger_inputs` (macro for `TIM4_IRQHandler`) | debounce of the Rotate and Lock jacks (`do_ROTUP`, `do_ROTDOWN`, `do_LOCK135`, `do_LOCK246`) | 32000 | 3 / 2 |
| Main loop | `main.c` `main()` `while(1)` | everything in section 4.2 except `param_read_freq`, `update_motion`, `update_morph`, `param_read_q` | none: unthrottled | lowest (thread mode) |

Checked by grep: `param_read_freq` and `update_motion` have exactly one caller, the TIM10 handler
(`params.c`). `update_morph` has one caller, `filter.c` (per sample). `param_read_q` is called once per
block from `filter.c`. `update_spread`, `read_spread`, `process_rotateCV`, `process_scaleCV`,
`param_read_switches` are called only from the `main.c` loop. `NVIC_PriorityGroup_2` (`main.c`) gives
priorities 0 to 3, so the audio block cannot be interrupted by any of the others. **This corrects
M1-t**, which said `param_read_freq` runs in the main loop.

Boot order in `main()`: `set_default_param_values`, `I2S_Block_PlayRec` (audio starts), then
`init_freq_lpf`, `init_freq_update_timer`, `init_ENV_update_timer`, then the loop. Before the first
TIM10 tick, `freq_shift[]` is 0 and `freq_nudge[2..5]` is 0 (`set_default_param_values`, `params.c`
initialisers), which would make `c1` zero in `process_audio_block`.

### 11.2 Rates (derived, nothing measured)
- Clock tree (`system_stm32f4xx.c`): `PLL_M` 8, `PLL_N` 336, `PLL_P` 2, HSE 8 MHz (`stm32f4xx.h`
  default, no override in the `Makefile`), so SYSCLK 168 MHz; `HPRE_DIV1`, `PPRE2_DIV2`, `PPRE1_DIV4`.
- Timer clocks: 168 MHz for TIM9 and TIM10 (APB2), 84 MHz for TIM4 (APB1). This uses the ST rule
  "timer clock is 2 x PCLK when the APB prescaler is not 1". That rule is in ST's reference manual,
  not in this repo, and I have not read it this session. The code comments agree with it for TIM10
  (`params.c`: "168MHz / (Period+1)"), TIM9 (`envout_pwm.c`: "168MHz / 30000") and TIM3
  (`envout_pwm.c`: "168M / 2 / 4096"). The TIM4 comments (`dig_inouts.c`: 16000 is 5.5 kHz,
  0.17778 ms) fit a 90 MHz timer clock, which matches the commented-out `PLL_N 360` (180 MHz) line in
  `system_stm32f4xx.c`; they are stale.
- Update period is register value + 1 counts (ST rule, same caveat; `params.c` says the same for TIM10).

| Source | Counts per update | Rate | Per audio block at exactly 96 kHz |
|---|---|---|---|
| TIM10 | 8196 at 168 MHz | 20,497.8 Hz | 28000/8196 = 7000/2049 = 3.4163 |
| TIM9 | 30001 at 168 MHz | 5,599.8 Hz | 28000/30001 = 0.9333 |
| TIM4 | 32001 at 84 MHz | 2,624.9 Hz | 14000/32001 = 0.4375 |
| Audio block | 16 frames | 6000 /s | 1 (= 28000 counts at 168 MHz) |

The audio rate is not exactly 96 kHz. `system_stm32f4xx.c` sets `PLLI2S_N` 344, `PLLI2S_R` 2, I2S source
PLLI2S, so I2SCLK = 8/8 x 344 / 2 = 172 MHz. `I2S_Init` in `stm32/periph/src/stm32f4xx_spi.c` (MCLK
output enabled, `codec.c` `Codec_AudioInterface_Init`; `I2S_EXTERNAL_CLOCK_VAL` is commented out in
`stm32f4xx_conf.h`, so the PLLI2S branch is used) computes `tmp = (172e6/256)*10/96000 + 5` in integers = 74, then
divider 7 (`i2sdiv` 3, `i2sodd` 1). Fs = 172e6 / (256 x 7) = **95,982.14 Hz** (the 256 x divider
formula is from ST's manual, not read here). That is 0.019 % below 96,000, or 0.32 cent flat against
the coefficient tables (all generated for 96000: first line of each `filter_coefs/*.h` says `// 96000`).
See M2-b in `open-questions.md`.

### 11.3 Design
All time in the core is counted in firmware time, never in host time.

- One call to the core processes exactly one block: 16 frames at the core rate (`processBlock`).
- After each block the scheduler adds the counts that block lasted to one accumulator per source and
  runs every tick that fell due, in this fixed order: TIM9 ticks, TIM10 ticks, main-loop iterations.
  The three are independent (`update_ENVOUT_PWM` reads `channel_level`, `ENVOUT_preload`,
  `envspeed_*`, which `param_read_freq` and `update_motion` do not write), so the order between TIM9
  and TIM10 does not matter; both run before the main loop because they preempt it.
- Ticks run between blocks, never inside one. This matches the firmware: the audio ISR has the
  highest priority.
- Exact integer arithmetic, no floating-point drift. Core rate is a rational `Fs = num/den`
  (96000/1 nominal; 671875/7 for the derived 95,982.14). Counts per block at 168 MHz =
  2,688,000,000 x den / num. Each source with period P counts keeps `acc` in units of 1/num counts:
  `acc += 2,688,000,000 * den`; `while (acc >= P * num) { acc -= P * num; run_tick(); }`. All values
  fit in 64-bit integers. At nominal 96 kHz TIM10 runs 7000 ticks in every 2049 blocks.
- Main-loop iterations use the same accumulator with P = 1/loop_rate. **The source gives no loop
  rate.** Placeholder 12,000 iterations/s, read from one comment: `params.c` "CLEAR COARSE TUNING ...
  (~3s)" next to `lock_down[] > 36000`. That is the author's estimate, not a measurement (M2-c).
- `reset()` mirrors `main()`: defaults, tables, `init_freq_lpf`, `spread = (adc >> 8) + 1`
  (`main.c`), then one TIM10 tick and one loop iteration before block 0. In the firmware the first
  TIM10 timer starts a few instructions after audio starts, so the first TIM10 tick very likely
  precedes the first block (derived from `main()` order, not measured). This answers M1-t for
  `freq_shift` and `freq_nudge`: no block ever sees them at zero. See M2-d.
- Single thread. The firmware races (main loop vs TIM10 on `motion_*`, `note[]`) become a fixed order.
  All core calls come from the audio thread; the plugin copies parameters in between blocks.

### 11.4 Not modelled (each needs a capture to settle)
- Coalescing. The audio block takes about 78 us (static) or 104 us (morphing) of every 166.7 us
  (`filter.c` comment at `process_audio_block`). TIM10 fires every 48.8 us, so up to two TIM10
  updates can arrive during one block and merge into one pending interrupt. The real TIM10 rate
  under load is therefore below 20.5 kHz by an amount that depends on handler run time, which the
  source does not give (M2-e).
- Jitter of tick position inside the 166.7 us gap.

### 11.5 What depends on tick rates
- Freq jack smoothing: 40-tap moving average per tick (`params.c` `init_freq_lpf`, `lpf.c`
  `apply_fir_lpf`), 1.95 ms nominal, then bracket of 2 counts (`apply_bracket`).
- `update_motion`: starts a morph or spread; timing error under 50 us.
- Env Out followers (`update_ENVOUT_PWM`): time constant 1/(1-a) ticks: Fast attack 0.9990 is 1000
  ticks (0.179 s), Fast decay 0.9991 is 1111 (0.198 s), Slow attack 0.9995 is 2000 (0.357 s), Slow
  decay 0.9999 is 10000 (1.79 s), all at 5599.8 Hz. Trigger hold 285 ticks is 50.9 ms (comment
  says about 100 ms; M1-j).
- Main loop: `process_scaleCV` low-pass 0.99 per iteration (time constant 100 iterations),
  `read_spread`, `process_rotateCV`. Hold timers (`lock_down`, `ROTARY_BUTTON_HOLD`) count
  iterations; they belong to Milestone 7.

## 12. Sample-rate handling (Milestone 2 design, not yet implemented)

### 12.1 Why the core stays at 96 kHz
Everything numeric is tied to 96 kHz: the coefficient tables (`// 96000`), the decay constants
(`filter.c`: "c0 = 1 - 2/(decay * samplerate)"), the empirical amplitude and two-pass calibration
terms (`twopass_calibration.h`, `filter.c` `ratio_b`), the 16-sample block, and the timers above.
`globals.h` defines `SAMPLERATE 96000` with a comment that other values are for debugging only. Running
the core at another rate would change pitch, decay time and level. So the core is fixed and the host
rate is converted at the boundary. Core rate is one constant (`kCoreRateNum`/`kCoreRateDen` in `Constants.h`, now 671875/7 = 95,982.14 Hz, see M2-b).

### 12.2 Design
- Host rate equals core rate (96000): no conversion; only the 16-frame block adapter. The core rate is now
  95,982.14 Hz (M2-b, decided 2026-10-26), so a 96000 Hz host rate goes through the resampler (ratio 5375/5376); the
  one-to-one path (`BlockAdapter::prepareOneToOne`, `smr_render --core-rate`) is used for hardware comparisons.
- Otherwise: linear-phase polyphase FIR resampler, host to core on the way in and core to host on the
  way out, in `src/host` (plain C++, no JUCE). Fixed latency, reported to the host (Milestone 5).
- Block adapter: collects input frames until 16 are available, runs one core block, queues the
  output. Fixed pre-roll of one block (16 core frames, 167 us) so the output never underruns; that
  adds to the reported latency. Host buffer sizes of any length work (1 to thousands).
- Rational ratios for exact polyphase at the core rate 671875/7 (`ratioToCore`): 96000 is 5375/5376, 48000 is
  5375/2688, 44100 is 26875/12348, 88200 is 26875/24696, 192000 is 5375/10752 (the table has taps x up entries, up to
  26875). Host rates whose reduced `up` exceeds 30000 need an arbitrary-ratio path (interpolated filter table). Milestone 2's offline
  renderer supports only the exact list above and says so for other files; the real-time path in
  Milestone 5 handles any rate.
- Resampler quality targets (passband ripple, stopband attenuation, latency) are my design choices,
  not firmware facts. They are set and measured in Milestone 5. In Milestone 2 the renderer's
  resampler is for listening only.
- The hardware's own converter response (WM8731 filters, M1-l) is a separate question and is not
  emulated by this resampler.

### 12.3 Sample scale at the core boundary
The core takes and returns `int32_t` samples exactly as `process_audio_block` does: 24 significant
bits left-justified in 32 (`INPUT_LED_CLIP_LEVEL` 0x58000000, `SLIDER_CLIP_LEVEL` 0x50000000, trigger
level 0x02000000 all assume this). Filter state values are therefore of order 1e9. The conversion
float to int32 and back is plugin-side (M1-n decides the volts mapping). The firmware's 16-bit-pair
packing is transport and is not ported (M2-f).

## 13. Milestone 2 scope and rules

### 13.1 What is ported now
| Ported (core, line-by-line from the firmware) | Inputs only, logic deferred | Not in the core |
|---|---|---|
| `process_audio_block`: Two-Pass MAXQ, One-Pass MAXQ, One-Pass BpRe, bank and history logic (M1-p as is), morph blend, level smoothing, accumulation | `lock[]`, `lock_pressed[]`, `lock_down[]`, `q_locked[]`, `freqblock` (set directly; button state machines are Milestone 7) | LEDs, flash, `system_mode.c` |
| `param_read_q`, `param_read_freq` (both filter branches; LED-only statements omitted and marked), `param_read_switches`, `read_spread`, `process_rotateCV`, `process_scaleCV` | `ui_mode` fixed at PLAY | `rotary.c/.h` (GPLv3), codec, I2S, ADC, DMA |
| `update_morph`, `update_motion`, `update_spread`, `rotate_up/down`, `change_scale_up/down`, `jump_*` | Rotate and Lock trigger jacks as direct calls | `process_lock_buttons`, `process_rotary_*`, edit modes, `user_scales.c` edit UI |
| `update_ENVOUT_PWM`, `FreqCoef_to_PWMval` (`usat` becomes a clamp to 12 bits), `lpf.c` | switch states as booleans | `limiter.c`, `equal_pow_pan_padded.h` (unused) |
| Tables, unmodified | `user_scalebank` default data (`user_scales.c` `DEFAULT_user_scalebank`) | noise source (M1-n) |

One-Pass and BpRe are ported because they sit in the same function; whether the plugin exposes them
is the open v5 scope question and is not decided here.

### 13.2 Tables
Upstream table headers are included verbatim from `third_party/SMR`, with no copies and no edits,
inside a namespace in one core file (`filter_coefs/*.h`, `exp_4096.h`, `exp_1voct.h`, `log_4096.h`,
`twopass_calibration.h` contain no `#include`, checked by grep). At start-up they are copied with
`memcpy` into flat core-owned arrays: MAXQ 231 floats, BpRe 693 floats per table. This avoids indexing
a `[231][3]` array past its first row through a `float *` (`filter.c` does that; the headers fill only
the first 231 floats, M1 section 3.2). `twopass_calibration` is declared 3380 and has 3354
initialisers (M1 section 8); the highest index used is 3025.

### 13.3 Numeric rules
1. Every floating-point literal gets an `f` suffix. The firmware builds with
   `-fsingle-precision-constant` (`Makefile` `CFLAGS`), so each bare `0.003`, `1.4`, `10.0`, `4096.0`
   there is a float; on a desktop it would silently be a double.
2. `-ffp-contract=off` on every `src/core` and test target (Clang on arm64 may otherwise fuse
   multiply-adds). A test checks the flag took effect (V3).
3. `-O2`, no `-Ofast`, no fast-math. The firmware builds with `-Ofast` plus FMA-capable `fpv4-sp-d16`,
   so bit-exact match with hardware is not promised (M1 section 8); tolerances come in Milestone 3.
4. Same statement order and same float/int conversions as the C source, including the accumulation
   `filtered_buffer[i] += f_blended * channel_level[j]` (int32 converted to float, added, truncated
   back) and the channel order 0..5.
5. Where the C is undefined, the port uses a defined behaviour and lists it in `docs/port-notes.md`:
   float to int32 overflow (saturate, NaN gives 0; hardware overload behaviour still needs capture),
   `c_hiq`/`c_loq` before first assignment (NULL in the firmware; port starts at bank 0 tables),
   `usat`. Array indices checked against table sizes: `exp_4096` max index 3125, `twopass_calibration`
   max 3025, BpRe `scale*63 + nudge_filter_num*3 + 2` max 692.
6. No denormal flushing in the core. The firmware sources set only `CPACR` (FPU enable); I found no
   `FPSCR` write. Denormal handling for the plugin is Milestone 5.

### 13.4 Layout
`src/core`: `Tables`, `Engine` (state, `reset`, `processBlock`, `runDueTicks`), `Filter`, `Params`,
`Rotation`, `EnvOut`, `Lpf`, `Scheduler`. `src/host`: resampler and block adapter (offline use in M2).
`tools/smr_render`: WAV in, WAV out, controls as command-line numbers in ADC counts. `tests/`.

## 14. Milestone 2 verification plan (all in GitHub Actions) and decisions needed
- V1 Differential test: the unmodified firmware files (`filter.c`, `params.c`, `rotation.c`, `lpf.c`,
  `envout_pwm.c`) compiled on an Ubuntu runner with GCC and `-fsingle-precision-constant
  -ffp-contract=off -O2`, hardware replaced by a shim in `tests/oracle/` (test code only, never in
  `src/core`), compared bit for bit per block against the port over scripted stimulus (every bank,
  Q, morph, rotate, spread, scale CV, levels, both filter modes). This is the only check that
  the port matches the source. It is extra scope; say if you want it dropped.
- V2 Invariants: no NaN or infinity and bounded output over 60 s of noise, silence, loud sine and
  impulses, at default and at extreme controls, for all 20 banks in MAXQ and all 19 in BpRe.
- V3 Fused multiply-add check: a fixed expression whose fused and unfused results differ.
- V4 Cross-architecture: output hash of a fixed render on arm64 and on x86_64 (`arch -x86_64`,
  reported as skipped if the runner cannot run it).
- V5 Default resonator: impulse response of each default channel peaks within 1 % of the frequency
  computed from the table (65.40, 73.41, 82.40, 97.99, 109.99, 130.80 Hz, section 9). This tests the
  port against the table, not against hardware (Milestone 3).
- V6 Renderer end to end on synthetic WAVs (mono and stereo; 44.1, 48, 96 kHz).

Decisions needed before coding are listed as M2-a to M2-g in `open-questions.md`.
