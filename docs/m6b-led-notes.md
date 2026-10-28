# Milestone 6b: what the firmware does for its lights (reading notes, 2026-10-27)

Read so far: `third_party/SMR/led_ring.c`, `leds.c`, `pca9685_driver.c` (`LEDDriver_set_LED_ring`), parts of `filter.c`, `audio_util.c`, `globals.h`. Nothing is drawn yet. Not read yet: `envout_pwm.c` (range and update of `ENVOUT_PWM`), the full clip loop in `audio_util.c`.

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

## For the new chat
- The display needs a read-only copy of core state (`note[]`, `motion_fadeto_note[]`, `motion_morphpos[]`, `channel_level[]`, `lock[]`, `q_locked[]`, `freqblock`, `ENVOUT_PWM[]`, env mode) published from the audio thread without changing the audio (no new locks on the audio path, no change to src/core), read by the editor timer.
- Open decision for Bryn (M6-f): a scale and bank view on the ring after a Scale click or Bank change; the firmware shows it only in Scale mode.
