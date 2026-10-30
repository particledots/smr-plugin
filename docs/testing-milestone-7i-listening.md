# Milestone 7 listening sheet (existing behaviour, checked against the manual and the firmware source)

Build run 40987029109 (commit a683e44; install as in docs/testing-milestone-7h.md). Intel Mac first. This sheet checks things already built. Where I wrote "I expect", it comes from the manual or the source named; where I wrote "no expectation", I have not read the source for it and only want your report.
Use the Noise switch (stand-in) for a steady sound, or a track of noise. Defaults unless a step says otherwise. Wait 10 seconds after opening the window.

## 1. CV Slew
Manual: slew limits the incoming CV on the Level CV jacks; the amount follows Morph at the moment the switch is flipped on.
1. Morph 0%, Slew off. Move Level CV 1 from 0% to 100% in one click. Tell me whether it jumps.
2. Morph 60%, flip Slew on, repeat. I expect a slower rise. Tell me how many seconds it takes (rough).
3. Morph 0%, flip Slew on, repeat. No expectation: tell me what you hear.

## 2. Scale Rotation
Manual: with Scale Rotation on, channels rotate into the next or previous scale each time they rotate fully around.
1. Scale Rotation on, Morph low. Click Rotate CW 25 times, listening. Tell me whether the pitches move into a different scale at some point, and after how many clicks.
2. Same with Scale Rotation off. I expect the pattern to wrap around the same scale.

## 3. Level CV
Source (`filter.c`, level = Level CV x slider, below 0.7% is silent).
1. Slider 1 100%. Level CV 1: 100%, 50%, 10%, 0%. I expect the level to fall with it and channel 1 to go silent at 0%.
2. Slider 1 50%, Level CV 1 100%. I expect about the same as slider 100% with Level CV 50%. Tell me if it is clearly different.

## 4. Res (Q) CV
Manual: the Q knob offsets the Q CV.
1. Q knob at 0%. Res (Q) CV 0%, 50%, 100%. I expect the ringing to get longer each time.
2. Q knob at 100%, Res (Q) CV at 100%. Tell me if it behaves sensibly (no runaway, no silence).

## 5. Rotate CV
Source (`ParamMap.cpp formatValue`, `process_rotateCV`): 20 positions, count / 205, so about 5% per step; it reacts after a change of more than 100 counts (about 2.4%).
1. Rotate CV 0%, 5%, 10%, 15%, 20%, 50%, 100%. Tell me whether the pattern moves one position per 5% step.

## 6. Scale CV
Source: count / 409, 11 scales (0 to 10), about 9.1% per step, smoothed.
1. Scale CV 0%, 10%, 20% ... 100%. Tell me whether the scale changes about every 10% and how many different scales you hear.

## 7. Spread CV reset
Source (`params.c read_spread`): spread 1 to 16 from count / 256, with a hysteresis of 75 counts.
1. Spread 0%. Listen or note the six light colours' pattern on the ring. Move Spread to 100%, then back to 0%.
2. I expect exactly the same sound and pattern as the first time. Tell me if not.

## 8. Trigger threshold on Pre (docs/open-questions.md M7-m)
1. Env Out switch on Trig, Pre | Post on Post. Sound: noise on (stand-in). Set slider 1 so Env Out light 1 turns on and off with the noise level (move it until it is just lit).
2. Flip Pre | Post to Pre. Move slider 1 to 0%. I expect light 1 to behave as it did, not go dark, because the threshold was stored.
3. Flip back to Post. I expect the threshold to follow the slider again (light 1 goes dark at 0%). The manual says flipping back erases it; the source overwrites it. Tell me what you see.
4. Fresh instance, switch straight to Pre without ever going to Post. I expect half the signal to count (source: factor 0.5). No expectation on the light: tell me.

## 9. Env Out Trigger mode
Source (`envout_pwm.c`): high for at least 285 update passes after a trigger, low after the signal stays under the threshold for 285 passes.
1. Env Out on Trig. Play short noise bursts, then steady noise. I expect the lights to be on during a burst, a little longer than the burst, and on all the time with steady noise.
2. Tell me how long a light stays on after a very short burst (about 50 ms or about 100 ms; see M7-m item 3).

## 10. Freq knob, 135 | 1 and 246 | 6 switches
Manual: the 135 | 1 switch selects whether the Freq knob and the Freq Nudge knob control channels 1, 3, 5 or only channel 1; 246 | 6 selects channels 2, 4, 6 or only channel 6.
1. Switch on 135. Move the odds Freq knob. I expect channels 1, 3, 5 to move together.
2. Switch on 1. Move the odds Freq knob. I expect only channel 1 to move; 3 and 5 stay. Try the same with the odds Freq Nudge.
3. Switch on 6, even side: I expect only channel 6 to move; 2 and 4 stay.

## 11. Spread and Scale with a lock on
Manual: Rotation, Spread, Freq Nudge, Scale and Bank do not change when a channel is locked.
1. Lock 3. Move Spread from 0% to 100%, press Scale Up twice. I expect channel 3 to stay at its pitch; the others change.
2. Unlock 3. I expect it to follow again.

## 12. Bank names against manual p.24
Plugin list: Blue 1 to 6, Pink 1 to 3, Orange 1 to 4, Green 1 to 6, White (custom): 20 banks.
Manual (Scale Banks and Groups): Blue 6 banks (Major, Minor, Single Western Intervals, Western Triads, Chromatic, Whole Step ET); Pink 3 (Single Intervals, Western Triads, Whole Step, all Just Intonation); Orange 4 (Indian Pentatonic, Indian Shrutis, Mesopotamian, Gamelan Pelog); Green 6 (Alpha, Alpha more notes, Gamma, 17 notes, Bohlen-Pierce, 296 EQ); White 1 (custom).
1. Open the Bank list. I expect 20 entries in that group order. Tell me if the count or the group sizes differ.
2. Choose Blue 1, then Pink 1, Orange 1, Green 1, White. I expect the Env Out light colours to change per group (blue, pink, orange, green, white). Tell me the colours you see.

## Report
Per number: what you heard or saw, and where it differs from "I expect". Say the Mac and build run.
