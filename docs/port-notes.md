# Port notes (Milestone 2)

Where `src/core` differs from `third_party/SMR` (v5.1.1, commit 79bc953), and why. Anything not listed here
is meant to be the same statement in the same order. The V1 test (`tests/oracle`) compares the port with the
unmodified firmware files bit for bit; this file says what V1 cannot cover.

Nothing here is confirmed until the CI annotations say so; see `docs/status.md`.

## Numeric rules
- Every float literal has an `f` suffix (the firmware is built with `-fsingle-precision-constant`, `Makefile`).
- Build flags `-O2 -ffp-contract=off`. The firmware is built `-Ofast` on hardware with fused multiply-add, so
  a bit-exact match with the hardware is not possible. The oracle uses the same flags as the port, so V1
  compares the statements, not the hardware.
- Tables (`Tables.cpp`) are generated from the firmware's own arrays. V1a compares the tables the firmware
  really picks (found by running its own bank selection in `filter.c`) with the port's.

## Undefined behaviour made defined
- **P-3 Saturation.** Float to int32 and float to uint32 conversions saturate (NaN gives 0) in `f2i32`/`f2u32`.
  On ARM the firmware's `vcvt` does the same; x86 does not. `Engine::saturation_count` counts output saturations.
  V1 therefore compares only the blocks before the first output saturation (`valid_steps`).
- **P-4 `c_hiq`/`c_loq` start-up.** The firmware leaves them for the first `change_filter_type`/bank selection.
  `reset()` runs the same selection, so they are never null.
- **P-5 Index guards.**
  - Table indexes clamp (`clampIdx`).
  - `twopass_calibration[(uint32_t)(qval_b-900)]` reads the last entry when the index is past 3379.
    The firmware reads past the array here. `param_read_q` (`params.c`) lets `qval` overshoot 4095 by up to
    1/15 of a jump (`poll_ctr` reaches 16 and the divisor is 15), and `filter.c` then computes
    `qval_b = 1000 + (qc-3900)*15`. A `qval` above about 4118 therefore indexes past the array. A jump from an
    unlocked Q of about 3700 to 4095 reaches this. Found while building V1 (qval 4196 and 4203 at step 21894 of
    scenario 0). Listed in `docs/open-questions.md` as P-5.
  - `qval` itself is not clamped (an earlier version of the port clamped it to 4095; V1 showed that changes the
    audio, so it was removed). `log_4096[qval]` is only read for `qval <= 4065`.
- **P-6 Start-up order (M2-d).** `reset()` runs one main-loop pass and then one TIM10 tick before block 0.
- **P-7 Search-loop guards.** `update_spread` and `update_motion` have `while(!is_distinct)` loops with no
  bound. The port gives up after 20 steps and counts it in `loop_guard_trips`. No V1 scenario has tripped one.
- **P-8 Display-only state.** Milestone 2 left out `ongoing_fine_tuning`, `fine_envled`, `cur_envled_state`, `saved_envled_state`,
  `coarse_adj_led`, `old_switch_state` and the display `else if` chain of `param_read_freq`. **Milestone 7 step 3b ported them**
  (`src/core/Params.cpp`, members in `Engine.h`, `coarseLadder` also sets `cur_envled_state`; `ongoing_coarse_tuning`, `fine_tuning_timeout`
  and `fine_timer` with them). Nothing that feeds the audio or the Env Out jacks reads them. V1 compares all of them with the unmodified firmware
  (`tests/oracle/trace_common.h`), and scenario 8 (`gen_stimulus.cpp`) holds the Nudge controls still long enough for the 12000 and 25000 tick timeouts
  to run out. Times: TIM10 ticks, `Constants.h kDisplayTime*`. The light rules themselves (`led_ring.c calculate_envout_leds` COARSE and FINE TUNING
  branches, `leds.c update_lock_leds` flash) are in `src/plugin/DisplayModel.h`, not in the core. Not in the plugin at all: the scale display
  branch (rotary), `ui_mode` other than PLAY.
- `usat #12` in `FreqCoef_to_PWMval` is a clamp to 4095. In the oracle build, the same line is compiled through
  `-Dasm(...)=result=oracle_usat12(result)` (x86 cannot assemble `usat`).
- The coarse-tuning ladder is written twice in `param_read_freq`; the port shares it as `coarseLadder()` and keeps
  the quirk that `t == 4095` matches no branch.
- `rot_dir` is written but never read in the source; it is kept.

## Behaviour kept from the source on purpose
- M1-o: BpRe banks 0 to 18 only; no bank-19 branch.
- M1-p: the history clear zeroes floats 0..221 of `buf[i]` and never touches `buf_a`.
- M1-s: as listed in `docs/open-questions.md`.
- Output accumulate: `filtered_buffer[i] += f_blended*channel_level[j]` converts int to float and back, exactly as C does.

## Provisional (not from the source)
- Output mapping (M1-m): host left is `filtered_buffer` (channels 2/4/6), host right is `filtered_bufferR` (1/3/5).
- Level scale (M1-n): 1.0 is 2147483520.0f (full scale). Placeholder until a capture.
- Core rate 96,000 Hz (M2-b); main loop 12 kHz (M2-c).
- `ui_mode` is fixed at PLAY. Scale editing, calibration and the display are not ported.
- `filter_out_a/b` are zero-initialised (the firmware has them in `.bss`).

## The test oracle (tests/oracle)
- `shim/stm32f4xx.h`, `shim/arm_math.h`, `shim/shim_globals.c` stand in for the ST libraries and the firmware's
  other files so `filter.c`, `params.c`, `rotation.c`, `lpf.c` and `envout_pwm.c` compile unmodified on Ubuntu.
  They are test code only.
- Switch pins are set through `oracle_set_pins` (GPIOD.8 = !env_pre, GPIOD.15 = !mod246_six, GPIOD.14 = range,
  GPIOD.9 = !rotary_sw, GPIOB.2 = !mod135_one, GPIOB.3 = !env_slow, GPIOE.8 = cvlag, GPIOE.9 = !env_fast).
- Ticks are carried in the stimulus, so V1 does not cover the scheduler; the scheduler has its own test.
- Stimulus limits, because the oracle's behaviour is undefined outside them: Q controls move slowly and are capped
  at 3700 in two-pass scenarios (P-5); banks stay 0..19; scenarios 4 and 7 saturate and are compared up to that block.

- P-10: one-pass BpRe selected while a channel is on bank 19 and the previous pointers were MAXQ: out-of-range table read in `filter.c`. See docs/open-questions.md P-10.
