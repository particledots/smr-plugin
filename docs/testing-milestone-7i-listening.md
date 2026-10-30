# Milestone 7 listening sheet (v3, 2026-10-29)

This sheet is about the PLUGIN in Logic only. You do not need your real SMR.
Intel Mac first. Build run 40987029109 (commit a683e44).

## How to read this sheet

Every step is one action. Words in "quotes" are names exactly as they appear in the plugin's Controls list.

The plugin window has a **View** menu at the top. It has two entries:
- **Editor**: the picture of the SMR (sliders, knobs, switches, lights, buttons).
- **Controls**: a plain list of every parameter, each with a slider and a number box.
If there is no View menu, stop and tell me.

**"Set X to Y"** always means: View menu, Controls. Find X in the list. Drag its slider until its number box reads Y.
- A slider at the far left reads 0 %. At the far right it reads 100 %.
- Four controls do not show percent: "Rotate CV" shows 0 to 19, "Scale CV" shows 0 to 10, "Spread" shows 1 to 16, and switches show words.

Level CV 1 to 6, "Res (Q) CV", "Rotate CV" and "Scale CV" exist ONLY in the Controls list. The lights exist ONLY in the Editor.

## Start (do this once, and again whenever I say "new instance")

1. Insert a NEW Unofficial SMR on a track. Wait 10 seconds after its window opens.
2. View menu, Editor.
3. Click the switch labelled "Noise (stand-in)" (left side, under "Scale rotation") so it is ON. You hear a steady sound.
4. View menu, Controls.
5. Set "Level 2" to 0 %. Do the same for "Level 3", "Level 4", "Level 5" and "Level 6". Now only channel 1 sounds.

## 1. CV Slew

Test A: Slew off
1. Set "Morph" to 0 %.
2. Set "CV Slew" to Off.
3. Set "Level CV 1" to 0 %. The sound goes silent.
4. Drag the "Level CV 1" slider all the way right in one quick movement.
5. Tell me: does the sound come back at once, or fade up?

Test B: Slew on, Morph 60 %
1. Set "Morph" to 60 %.
2. Set "CV Slew" to On.
3. Set "Level CV 1" to 0 %. The sound goes silent.
4. Drag the "Level CV 1" slider all the way right in one quick movement.
5. Tell me roughly how many seconds the sound takes to reach full level. I expect it to take longer than in Test A.

Test C: Slew on, Morph 0 %
1. Set "Morph" to 0 %. Leave "CV Slew" On.
2. Set "Level CV 1" to 0 %.
3. Drag the "Level CV 1" slider all the way right in one quick movement.
4. Tell me what you hear. No expectation.

## 2. Scale Rotation

1. Set "Level 2" to "Level 6" back to 100 % (one at a time). Set "Morph" to 0 %.
2. Set "Scale Rotation" to On.
3. View menu, Editor. Click the right-hand "Rotate trig" button once, then wait one second. Do this 25 times in total, listening.
4. Tell me: did the pitches move into a different scale at some point? After how many clicks?
5. View menu, Controls. Set "Scale Rotation" to Off.
6. View menu, Editor. Click the same button 25 times again, one click per second.
7. I expect the pitches to wrap around the same scale. Tell me what you hear.

## 3. Level CV

Start: Set "Level 2" to "Level 6" to 0 % again, so only channel 1 sounds.
1. Set "Level 1" to 100 %.
2. Set "Level CV 1" to 100 %. Listen.
3. Set "Level CV 1" to 50 %. Listen.
4. Set "Level CV 1" to 10 %. Listen.
5. Set "Level CV 1" to 0 %. Listen.
6. I expect the sound to get quieter at each step and to be silent at 0 %. Tell me if not.
7. Set "Level 1" to 50 % and "Level CV 1" to 100 %. Listen.
8. Set "Level 1" to 100 % and "Level CV 1" to 50 %. Listen.
9. I expect steps 7 and 8 to sound about the same loudness. Tell me if they are clearly different.

## 4. Res (Q) CV

1. Set "Level 2" to "Level 6" back to 100 %.
2. Set "Res (Q) Knob" to 0 %.
3. Set "Res (Q) CV" to 0 %. Listen to how long the sound rings.
4. Set "Res (Q) CV" to 50 %. Listen.
5. Set "Res (Q) CV" to 100 %. Listen.
6. I expect the ringing to get longer at each step.
7. Set "Res (Q) Knob" to 100 % and leave "Res (Q) CV" at 100 %.
8. Tell me if it sounds sensible (no runaway loudness, no silence).

## 5. Rotate CV

1. Set "Res (Q) Knob" and "Res (Q) CV" back to 0 %.
2. "Rotate CV" shows a number from 0 to 19. Set "Rotate CV" to 0. Listen for 2 seconds.
3. Set it to 1. Listen for 2 seconds. Then 2, then 3, then 4, then 10, then 19, 2 seconds each.
4. Tell me: did the sound change at each new number? Did the pattern move by one position for each step of the number?

## 6. Scale CV

1. Set "Rotate CV" back to 0.
2. "Scale CV" shows a number from 0 to 10. Set "Scale CV" to 0. Listen for 2 seconds.
3. Set it to 1, then 2, then 3, and so on up to 10, listening 2 seconds at each.
4. Tell me: does the scale change at each number? How many different scales did you hear?

## 7. Spread comes back to the same sound

1. Set "Scale CV" back to 0.
2. "Spread" shows a number from 1 to 16. Set "Spread" to 1.
3. View menu, Editor. Listen, and look at the ring of small lights around the Rotate dial. Remember what the ring looks like.
4. View menu, Controls. Set "Spread" to 16.
5. Set "Spread" back to 1.
6. View menu, Editor. Compare the sound and the ring with step 3. I expect them to be exactly the same. Tell me if not.

## 8. Trigger threshold, Pre and Post (manual p.18)

1. View menu, Controls. Set "Fast | Trigger | Slow | 1V/oct Switch" to Trigger.
2. Set "Pre | Post Switch" to Post.
3. Set "Level 2" to "Level 6" to 0 %. Set "Level 1" to 100 %. Noise stays ON.
4. View menu, Editor. Look at the row of six round lights under the sliders, labelled "Env out". Find light 1 (the leftmost). It should be lit.
5. View menu, Controls. Drag "Level 1" down slowly until light 1 (Editor) just goes dark. Then raise "Level 1" a little until light 1 just lights again. Leave "Level 1" there.
6. Set "Pre | Post Switch" to Pre.
7. Set "Level 1" to 0 %.
8. View menu, Editor. I expect light 1 to stay as it was in step 5 (the threshold was stored). Tell me what you see.
9. View menu, Controls. Set "Pre | Post Switch" back to Post.
10. View menu, Editor. I expect light 1 to go dark, because the threshold follows "Level 1" again. (Manual p.18 says the opposite direction erases it; the firmware overwrites it in Post.) Tell me what you see.
11. Do "Start" again with a NEW instance. Set "Fast | Trigger | Slow | 1V/oct Switch" to Trigger and "Pre | Post Switch" to Pre immediately, never using Post. Tell me whether any Env out light is on. No expectation.

## 9. Env Out Trigger mode

1. Use the instance from the Start section, Noise ON. Set all six Levels to 100 %. Set "Fast | Trigger | Slow | 1V/oct Switch" to Trigger and "Pre | Post Switch" to Post.
2. View menu, Editor. I expect all six Env out lights to stay on with the steady noise. Tell me what you see.
3. Click the "Noise (stand-in)" switch so it is OFF. Set "Res (Q) Knob" to 0 %.
4. Play a short drum hit or click from a region on the track.
5. I expect each Env out light to flash on with each hit. Tell me what you see.
6. Not asked: how long a light stays on. It cannot be judged by eye (open item M7-m).

## 10. Freq knob with the "135 | 1" and "246 | 6" switches

Start with a NEW instance (Start section, but in step 5 set only "Level 2", "Level 4", "Level 5" and "Level 6" to 0 %, so "Level 1" and "Level 3" sound).
1. Set "135 | 1 Switch" to 135 (all odd channels).
2. View menu, Editor. Slowly turn the left "Freq" knob (the one below the top left knob). Listen.
3. I expect both pitches (channels 1 and 3) to move together. Tell me what you hear.
4. View menu, Controls. Set "135 | 1 Switch" to 1 (channel 1 only).
5. View menu, Editor. Turn the same left Freq knob again. I expect only channel 1 to move. Channel 3 stays.
6. Turn the top left "Freq Nudge" knob. I expect the same: only channel 1 moves.
7. View menu, Controls. Set "Level 1" and "Level 3" to 0 %. Set "Level 4" and "Level 6" to 100 %.
8. Set "246 | 6 Switch" to 6 (channel 6 only).
9. View menu, Editor. Turn the right "Freq" knob. I expect only channel 6 to move. Channel 4 stays.

## 11. Spread and Scale with a Lock on

1. View menu, Controls. Set "Level 1" and "Level 3" to 100 %. Set all other Levels to 0 %. Set "Spread" to 1.
2. View menu, Editor. Click the round Lock button above slider 3 so it lights.
3. View menu, Controls. Set "Spread" to 16.
4. View menu, Editor. Click the right-hand "Scale" button twice.
5. I expect channel 3 to stay at its pitch while channel 1 changes. Tell me what you hear.
6. Click the Lock button above slider 3 again to unlock it.
7. View menu, Controls. Set "Spread" back to 1.
8. I expect channel 3 to follow again. Tell me what you hear.

## 12. Bank names against manual p.24

The manual lists 20 banks: Blue 6, Pink 3, Orange 4, Green 6, White 1.
1. View menu, Editor. Click the "Bank" pop-up in the bottom row (the third pop-up, with the dashed outline).
2. Tell me how many entries it lists, and write down the names in order.
3. Choose Blue 1. Listen. Then Pink 1, Orange 1, Green 1, White, one after another.
4. Tell me only whether each choice changes the sound. Group colours and the bank flash from manual p.24 are not built (your decision, 2026-10-29); do not expect them.

## Report

Per number (for example "5.4"): what you heard or saw, and where it differs from "I expect". Say the Mac and the build run.
