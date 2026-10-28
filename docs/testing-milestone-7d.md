# Milestone 7, step 3b test: Env Out lights for fine tune and transposition, Lock button flash

New CI build needed. Install it as usual (docs/INSTALL.txt) once CI has passed; I give you the run number. Test on the Intel Mac first. Tell me when you also test on the M4 mac mini.

## What the firmware does (read from `led_ring.c calculate_envout_leds`, `leds.c update_lock_leds`, `params.c param_read_freq`, manual p.11-12)
- Turn a Freq Nudge knob: the Env Out lights of the channels that knob controls go **white (no nudge) toward blue (a little over a semitone)**. Channels the knob does not control are off. A channel that is locked, or switched off by the 135 | 1 or 6 | 246 switch, is **dim** instead.
- A channel that has just been enabled or disabled blinks twice (switch flip, Lock press) before it settles.
- The lights go back to the normal Env Out display after the knob stops: about 0.6 s one knob, about 1.2 s if both knobs were moved. (The manual says "after a second"; open-questions M7-i.) The window redraws these lights about 6 times a second, like the module, so add up to 0.2 s.
- Hold a channel's Lock button and turn that side's Nudge: **red lights = transposed down, blue = up**, one light per semitone. No transposition (0) shows three red (lights 1 to 3) and three blue (lights 4 to 6). +n lights the first n lights blue; -n lights the last n red. They go back to normal soon after you let go.
- Hold all six Lock buttons: after the hold time the Lock buttons of transposed channels **flash**; releasing a button clears that channel's transposition (a locked channel keeps its transposition until unlocked).
- None of this appears in BpRe filter type. Keep the filter type at the default (MaxQ).

## 0. Setup
1. Start state: switches at 1.3.5 and 2.4.6, all six Lock buttons dark, filter type default. Drag both Freq Nudge knobs fully up then fully down.
2. Watch the six Env Out lights, not the ring.

## 1. Fine tune lights
1. Drag Nudge odds slowly from 0% to 100%. Expect lights 1, 3, 5 white at 0% fading to blue at the top; lights 2, 4, 6 off.
2. Stop. Count seconds. Expect the lights to return to normal in about 1 s or less. Tell me roughly how long.
3. Same for Nudge evens: lights 2, 4, 6 change, 1, 3, 5 off.
4. Move both knobs at once for a few seconds, then stop. Tell me whether it takes longer to go back than with one knob.

## 2. Switches and Locks
1. Switch 135 | 1 to "1" while the lights are normal. Expect lights 3 and 5 to blink twice, then settle dim while you turn Nudge odds; light 1 changes white to blue.
2. Flip back to 135. Expect 3 and 5 to blink twice and go bright (they can be controlled again).
3. Click Lock 4 (so it lights). Turn Nudge evens. Expect light 4 dim, lights 2 and 6 white to blue. Click Lock 4 again to unlock.

## 3. Transposition lights
1. Shift-click Lock 1 (held). Nudge odds at 0%. Wiggle it slightly. Expect lights 1 to 3 red and 4 to 6 blue (zero transposition).
2. Drag Nudge odds up past the middle, slowly. Expect the blue lights to grow from light 1: one blue light at +1, six at +6 (top). Tell me the pattern at +1, +3 and +6.
3. Drag down through the middle to the bottom. Expect red lights to appear from light 6 backwards: one at -1, six at -6. Tell me the pattern at -1, -3 and -6.
4. Release Lock 1 (shift-click). Expect the lights to return to normal within about 0.3 s, with no wait.
5. Hold Lock 1 again and only wiggle Nudge odds. Expect the lights to show the transposition you set, without changing it.

## 4. Several channels at once
1. Hold Lock 1, Lock 3, Lock 5 together (shift-click each). Turn Nudge odds to +3. Expect the +3 pattern. Release all. Tell me whether channels 3 and 5 follow.

## 5. Lock button flash while a clear is staged
1. Give channel 1 a transposition of +3 and channel 2 a transposition of -2 (hold its Lock, turn Nudge evens). Release both.
2. Shift-click all six Lock buttons and start a stopwatch at the last click. Do not move any knob.
3. Tell me: how many seconds until Locks 1 and 2 start to flash (the others must stay dark), and whether Lock 4 (Q-lock, if you set one) stops its flicker. Expect about 3 s at the 12 kHz placeholder (open-questions M1-c: manual says 2 s).
4. Release Lock 3, 4, 5, 6 first, then Lock 1. Tell me whether Lock 1 stops flashing and channel 1 goes back to its untransposed pitch (by ear: it drops by 3 semitones).

## 6. Top of the Nudge range (open-questions M7-h)
1. Nudge evens from 80% to 100% slowly with Lock 2, 4, 6 not held. Watch light 2. Tell me: does it go from white through blue and stay blue at the top, or does it turn white again in the last 6%, or go dark?

## 7. Not tested here
BpRe filter type shows none of these lights; I have not asked you to check it.
