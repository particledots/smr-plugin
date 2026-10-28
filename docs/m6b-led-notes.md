# Milestone 6b: what the firmware does for its lights (reading notes, 2026-10-27)

Read: `third_party/SMR/led_ring.c`, `leds.c`, `pca9685_driver.c` (`LEDDriver_set_LED_ring`), parts of `filter.c`, `globals.h`, and (2026-10-27, second pass) all of `envout_pwm.c`, all of `audio_util.c`, `dig_inouts.h` LED pins, `params.c TIM1_UP_TIM10_IRQHandler`, `main.c` loop, `rotation.c update_motion`. Nothing is drawn yet. Section "Second pass" at the end holds what was unread.

## Colours
- `led_ring.c DEFAULT_COLOR_CH[16][6][3]`, scheme `cur_colsch = 0` (default): ch1 {0,0,761}, ch2 {0,770,766}, ch3 {0,766,16}, ch4 {700,700,700}, ch5 {763,154,0}, ch6 {766,0,112}. Blue, teal, green, white, orange, pink, the colours of the Env Out lights in the panel photo.
- Order is red, green, blue: `LEDDriver_set_LED_ring` comments for channel 6: `env_out[5][2]` blue, `[1]` green, `[0]` red. Values are clamped to 1023 in the code; the driver is 12 bit, so 1023 is about a quarter of full duty.
- Bank colours: `SCALE_BANK_COLOR[30][3]` (blue shades, pink, yellow/orange, green, white).

## Light ring in play mode (`led_ring.c display_filter_rotation`)
- Brightness `t_f = max(channel_level[chan], 0.05)`.
- Unlocked channel: LED at `note[chan]` gets colour x `(1 - motion_morphpos[chan]) * t_f`; LED at `motion_fadeto_note[chan]` gets colour x `motion_morphpos[chan] * t_f`. Locked channel: its LED flashes between `t_f` and 0 (the `flash` variable toggles on every call).
- Blocked notes (`freqblock` bit set) show 75,75,75.
- Only the first channel (lowest index) whose `note[chan] == i` is drawn at ring position i (the loop sets `chan = 6`, a break). Sums are clamped to 1023. A literal port must keep this.
- The ring is updated when `update_LED_ring` has counted past 2000 passes or `flag_update_LED_ring` is set (`led_ring.c update_LED_ring`).

## Env Out lights (`calculate_envout_leds`)
- Play mode: colour x `ENVOUT_PWM[chan] / 4096`. In 1V/oct mode (`ENV_VOLTOCT`): the same with a floor of 0.05.
- Other cases need UI state the plugin does not have (rotary pressed or turned, coarse and fine tuning, SELECT_PARAMS, EDIT_COLORS, EDIT_SCALES).

## Slider LEDs and clip (`leds.c update_slider_LEDs`, `filter.c`)
- Play mode: brightness from `channel_level[i]` by software PWM, maximum 25 % (`f_slider_pwm` restarts at 4.0, "1.0/4.0 = 25% PWM max brigtness").
- Clip: in `filter.c process_audio_block`, if `slider_led_mode == SHOW_CLIPPING` and `ui_mode == PLAY` and `f_blended * channel_level[j] > SLIDER_CLIP_LEVEL` (0x50000000), `LED_SLIDER_ON` for that slider; the PWM cycle turns it off again. `slider_led_mode` defaults to `SHOW_CLIPPING` (`leds.c`).

## Lock buttons (`leds.c update_lock_leds`, play branch)
- Lit when `lock[i]`. When `q_locked[i]`, the state inverts once in every 16 updates (`flash1` counts 0 to 15; inverted when it is 0). Coarse-tuning clear flashing is a v5 UI feature, not in the plugin.

## Input clip LED (`audio_util.c`)
- `LED_CLIPL_ON/OFF` and `LED_CLIPR_ON/OFF` by comparing a sample against `INPUT_LED_CLIP_LEVEL` (0x58000000, about 9.6 V per the manual). Two LED pins in the code, one LED between the IN jacks on the panel (manual, Controls and Jacks). The exact on/off logic in that loop has not been read in full.

## Not in the firmware: In/Out meters
- Sketch v3 has In and Out level bars. The module has none; only the clip LED. They would be computed in the plugin from the host audio buffers and are a plugin-only addition (dash them like the other plugin-only controls).

## Things the plugin does not have
- Scale mode (`params.c process_rotary_button`: a tap sets `change_scale_mode`, which switches the ring to `display_scale`), the rotary press, System Mode. `src/core/Engine.cpp runLoopPass` lists them as not ported. So 6b can show the play-mode ring; a scale or bank view would be a plugin design choice, not the firmware's.

## Second pass (2026-10-27)
**Env Out value, `envout_pwm.c update_ENVOUT_PWM` (already ported: `src/core/EnvOut.cpp`, state `ENVOUT_PWM[6]`, 0 to 4095, 12 bit).** Called from `ENV_update_IRQHandler` (TIM9, 168 MHz / 30000 = 5.6 kHz). Fast and Slow: one-pole filter of `ENVOUT_preload` (attack and decay `envspeed_attack`/`envspeed_decay`, set in `params.c param_read_switches`: Fast 0.9990/0.9991, Slow 0.9995/0.9999); Pre: `envelope >> 18`; Post: `(envelope * channel_level) >> 16`. Trigger: `ENVOUT_PWM` is 4090 for about 285 ticks (about 100 ms) once `ENVOUT_preload` passes 0x02000000, else 0 after 285 low ticks. 1V/oct: `FreqCoef_to_PWMval`, minus 127.96875 (C2 = 1 V). So the lights follow the same number the jack PWM uses; the plugin reads `ENVOUT_PWM`, nothing is recomputed.
**Env Out light level (confirms the first pass):** `led_ring.c calculate_envout_leds`: play mode colour x `ENVOUT_PWM/4096`; 1V/oct mode the same with a floor of 0.05 (`dimamt`). Both clamp each component to 1023.
**Input clip LED, `audio_util.c audio_convert_2x16_to_stereo24`:** for every sample, left: `LED_CLIPL_ON` if sample > `INPUT_LED_CLIP_LEVEL` (0x58000000, `globals.h`), else `LED_CLIPL_OFF`; right likewise with `LED_CLIPR_*`. No absolute value, no hold: positive samples only, and the pin follows the last sample of each conversion (16 frames per block, `filter.c`). Pins: `dig_inouts.h LED_INCLIPL` GPIO_Pin_7 on `LED_GPIO`, `LED_INCLIPR` GPIO_Pin_7 on GPIOC (two pins). The panel photo and manual show one clip LED between the IN jacks; how the two pins reach it is not in the source I read. 0x58000000 is 0.6875 of the 32-bit range; with the plugin's placeholder scale (1.0 maps to the largest int32 below 2^31, `BlockAdapter.h`, M1-n) that is a host sample of 0.6875 (about -3.25 dBFS).
**Slider clip, `filter.c process_audio_block` end of block (lines 740 to 746):** uses `filter_out[j][0]` and `filter_out[j+6][0]` blended by `motion_morphpos[j]`, times `channel_level[j]`, against 0x50000000 (0.625 of range). `src/core/Filter.cpp` line 486 omits the LED call (a comment says so), so the plugin has to compute the same test from public `EngineState` (`filter_out`, `motion_morphpos`, `channel_level`).
**Slider LED brightness, `leds.c update_slider_LEDs`** (called from `params.c TIM1_UP_TIM10_IRQHandler`, TIM10, 168 MHz / 8196 = 20.5 kHz): `f_slider_pwm` falls by 0.08 per call from 4.0, so a cycle is 50 calls (2.4 ms). The LED is switched on when `channel_level >= f_slider_pwm`, so on-time per cycle is `channel_level / 4` (25 % at level 1.0). The clip call `LED_SLIDER_ON` switches the LED on regardless and the next cycle reset switches it off: a clipping block gives full on for up to one cycle, 4 times the brightest normal level.
**Ring and lock refresh rate depends on an unknown:** `led_ring.c update_LED_ring` redraws every 2001 main-loop passes or when `flag_update_LED_ring` is set (`main.c` start-up, `rotation.c update_motion` when a morph ends). The main-loop rate is not in the source (M2-c placeholder 12000 per second, giving about 6 redraws per second). The locked-channel ring flash toggles once per redraw, and the lock-button flash (`leds.c update_lock_leds`, `QLOCK_FLASH_SPEED` passes) depends on it too.
**Panel photo (project file, 1792958230567_image.png):** one yellow clip LED between the IN jacks, six slider LEDs inside the slider handles (white), six Env Out lights above the Env Out jacks in blue, teal, green, white, orange, pink.
**Plugin limit:** the plugin can only look at core state after each host block; the core runs several 16-frame blocks inside one host block (`BlockAdapter::process`). Clip tests on state read once per host block miss clips that end before the last core block of that block. A per-core-block look would need the adapter called in smaller pieces (an audio-path change, not done).

**Ring geometry (from `led_ring.c display_scale`):** the comment "ring positions 15 16 17 18 19 0 1 2 3 4 5" is the top half (the 11 scales; the manual says they are in the top half), and `ring[13-i]` for the six channels is the bottom six. So index 0 is at the top. Clockwise or counter-clockwise is not in the source; the panel photo (blue at 3 o'clock, colours continuing down and round to pink) suggests index grows clockwise. Not confirmed on the hardware (open-questions M6-i).

## Built in 6b (2026-10-27, code written; see docs/status.md for what is confirmed)
- `src/plugin/DisplayModel.h`: the rules above as plain functions (`ringLeds`, `envOutLeds`, `sliderLedDuty`, `sliderClipBits`), the colour table, and `DisplayHub` (lock-free copy of core state from the audio thread, plus clip and peak accumulators). Nothing in `src/core` or `src/host` changed.
- `PluginProcessor.cpp processBlock`: after the host block, `fillDisplayData` copies the members and publishes; input peaks and the input clip test are read before the in-place processing, output peaks right after `adapter.process`. The audio calls are unchanged.
- `PluginEditor.cpp`: 30 frames a second; ring, Env Out lights, input clip LED and meters drawn in `Content::paint`; slider LED drawn in `SmrUi.h SmrLookAndFeel::drawLinearSlider` inside the thumb.
- Decisions by Bryn (2026-10-27): no scale or bank view on the ring (M6-f); meters as peak bars with a short hold; slider LED at the firmware's literal ratio (duty = level / 4, clip = full).
