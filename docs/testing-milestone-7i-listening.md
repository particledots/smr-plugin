# Milestone 7 listening sheet (rewritten 2026-10-29, v2)

This sheet is about the PLUGIN in Logic only. You do not need your real SMR for anything here.
Intel Mac first. Build run 40987029109 (commit a683e44).

## How to set a control: two views of the same plugin window

At the top of the plugin window there is a **View** pop-up menu. It has two entries:
- **Editor**: the picture of the SMR (sliders, knobs, switches, lights, buttons).
- **Controls**: a plain list of every parameter, each with a slider and a number box.

(Apple's Logic documentation describes the View menu this way. I have not tried it in your Logic 10.7.8.
If there is no View menu in your window, stop and tell me; do not work around it.)

Rule for this sheet: **I name the view each time.** Controls-list names are in "quotes", exactly as they appear in the list.
- Level CV 1 to 6, Res (Q) CV, Rotate CV and Scale CV are ONLY in the Controls list. They are not in the Editor.
- Lights (the six Env out lights, the ring) are ONLY in the Editor.
- To set a value in the Controls list: drag the slider until the number box shows the value I give. (Typing a number into the box may also work; check that the box then shows the value I gave.)
- The number box shows "%" for most controls. Four are different: "Rotate CV" shows 0 to 19, "Scale CV" shows 0 to 10, "Spread" shows 1 to 16, and the switches show words.

## Setup, once, before section 1
1. Insert a NEW Unofficial SMR on a track (so every control starts at its default). Wait 10 seconds after opening the window.
2. View: Editor. Click the **Noise (stand-in)** switch (left side, under "Scale rotation") so it is ON. You now hear a steady sound. Playback can stay stopped.
3. View: Controls. Set "Level 2", "Level 3", "Level 4", "Level 5" and "Level 6" to 0 %, so only channel 1 sounds. (Sections 1 and 3 need this. Put them back to 100 % afterwards, or insert a new instance for later sections.)

## 1. CV Slew (channel 1 only)
1. View: Controls. "Morph" 0 %, "CV Slew" Off. Set "Level CV 1" to 0 %. Channel 1 should go silent. Now set "Level CV 1" to 100 % in one move. Tell me whether the sound jumps to full at once or rises.
2. "Morph" 60 %, "CV Slew" On. Repeat: "Level CV 1" 0 %, then 100 % in one move. I expect a slower rise. Tell me about how many seconds it takes.
3. "Morph" 0 %, "CV Slew" On. Repeat. No expectation: tell me what you hear.

## 2. Scale Rotation
1. Put all six Levels back to 100 % (View: Controls). "Morph" 0 %.
2. View: Editor. The **Scale rotation** switch is on the left, second from the top of the three switches under the Lock row. Turn it ON (check in the Controls list: "Scale Rotation" On).
3. View: Editor. Click the right-hand **Rotate trig** button 25 times, one click about every second, listening. Tell me whether the pitches move into a different scale at some point, and after how many clicks.
4. Turn "Scale Rotation" Off and click the same button 25 times. I expect the pitches to wrap around the same scale.

## 3. Level CV (channel 1 only: set "Level 2" to "Level 6" to 0 % again)
1. View: Controls. "Level 1" 100 %. Set "Level CV 1" to 100 %, 50 %, 10 %, 0 %. I expect the level to fall with it and channel 1 to be silent at 0 %.
2. "Level 1" 50 %, "Level CV 1" 100 %. I expect about the same loudness as "Level 1" 100 % with "Level CV 1" 50 %. Tell me if it is clearly different.

## 4. Res (Q) CV (all six Levels back to 100 %)
1. View: Controls. "Res (Q) Knob" 0 %. Set "Res (Q) CV" to 0 %, 50 %, 100 %. I expect the ringing to get longer each time.
2. "Res (Q) Knob" 100 %, "Res (Q) CV" 100 %. Tell me if it behaves sensibly (no runaway, no silence).

## 5. Rotate CV
1. View: Controls. "Rotate CV" shows a number from 0 to 19. Set it to 0, 1, 2, 3, 4, 10, 19, one after another, waiting 2 seconds each time.
2. Tell me whether the pattern moves one position for each step of the number.

## 6. Scale CV
1. View: Controls. "Scale CV" shows a number from 0 to 10. Set it to 0, 1, 2, ... 10, waiting 2 seconds each time.
2. Tell me whether the scale changes with each number and how many different scales you hear.

## 7. Spread (the Spread knob returns to the same sound)
1. View: Controls. "Spread" shows 1 to 16. Set it to 1. In View: Editor, note the sound and the pattern of the ring lights (the ring around the Rotate dial).
2. View: Controls. Set "Spread" to 16, then back to 1.
3. View: Editor. I expect exactly the same sound and ring pattern as in step 1. Tell me if not.

## 8. Trigger threshold, Pre and Post (manual p.18; docs/open-questions.md M7-m)
1. View: Controls. "Fast | Trigger | Slow | 1V/oct Switch" = Trigger. "Pre | Post Switch" = Post. Noise stays ON.
2. View: Controls. Lower "Level 1" until Env out light 1 (View: Editor, the row of six round lights under the sliders, labelled "Env out") just goes dark. Raise it a little until it just lights. Leave "Level 1" there.
3. Set "Pre | Post Switch" = Pre. Set "Level 1" to 0 %. I expect light 1 to stay as it was (the threshold was stored). Tell me what you see.
4. Set "Pre | Post Switch" = Post again. I expect the threshold to follow "Level 1" again, so light 1 goes dark at 0 %. (Manual p.18 says the opposite direction erases it; the firmware overwrites it in Post.) Tell me what you see.
5. Insert a second NEW Unofficial SMR, set Trigger, Noise ON, set "Pre | Post Switch" = Pre straight away without ever using Post. Tell me whether any light is on. No expectation.

## 9. Env Out Trigger mode
1. Noise ON, "Fast | Trigger | Slow | 1V/oct Switch" = Trigger, "Pre | Post Switch" = Post, all Levels 100 %. I expect the six Env out lights on all the time with steady noise.
2. Noise OFF, "Res (Q) Knob" 0 %. Play a short drum hit or click through the track. I expect each light to flash on with each hit.
3. Skipped: how long a light stays on (50 ms or 100 ms) cannot be judged by eye. It needs a recording; it stays open (M7-m).

## 10. Freq knob with the 135 | 1 and 246 | 6 switches (hear only the channels named)
Controls: "Level 1" and "Level 3" 100 %, all other Levels 0 %. Noise ON.
1. View: Controls. "135 | 1 Switch" = 135 (all odd channels). View: Editor: turn the left **Freq** knob (below the left Freq nudge knob). I expect both pitches (channels 1 and 3) to move together.
2. "135 | 1 Switch" = 1 (channel 1 only). Turn the same knob. I expect only channel 1 to move; channel 3 stays. Repeat with the left Freq **nudge** knob (the top left knob).
3. Now "Level 4" and "Level 6" 100 %, all other Levels 0 %. "246 | 6 Switch" = 6 (channel 6 only). Turn the right Freq knob. I expect only channel 6 to move; channel 4 stays.

## 11. Spread and Scale with a Lock on
1. "Level 1" and "Level 3" 100 %, all other Levels 0 %. View: Editor: click Lock button 3 (the round button above slider 3).
2. Set "Spread" (Controls list, 1 to 16) from 1 to 16, then click the right-hand **Scale** button (Editor, bottom row) twice. I expect channel 3 to stay at its pitch; channel 1 changes.
3. Click Lock button 3 again to unlock. Set "Spread" back to 1. I expect channel 3 to follow again.

## 12. Bank names against manual p.24
Manual groups: Blue 6 banks, Pink 3, Orange 4, Green 6, White 1 (custom) = 20.
1. View: Editor: click the **Bank** pop-up in the bottom row (the third pop-up, dashed outline). Tell me how many entries it lists and the names in order.
2. The window does not show the group colours or the bank flash from manual p.24 (your decision, 2026-10-29). Do not expect them. Choose Blue 1, then Pink 1, Orange 1, Green 1, White, and tell me only whether each choice changes the sound.

## Report
Per number (for example "5.2"): what you heard or saw, and where it differs from "I expect". Say the Mac and the build run.
