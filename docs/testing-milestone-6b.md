# Milestone 6b test: lights and meters

Test on the Intel Mac first (macOS 12.7.8, Logic 10.7.8). Tell me when you also test on the M4 mac mini. Play audio the whole time.
Install the newest build as in docs/testing-milestone-6a.md section 1. Write down the run number.

## 1. Look
1. The window shows: a ring of 20 dots in the middle (dark when off), six round Env out lights under the sliders, a small clip LED (dark yellow when off) at the left of the window, just right of the two In bars, about halfway down, two meters at the left and two at the right with dashed outlines, and a small LED inside each slider handle.
2. The second status line "triggers sent/seen" is still there (it stays until you say the M4 mac mini 6a test is done).

## 2. Ring (play noise or music through the plugin)
1. Fresh plugin, nothing moved: which ring dots are lit, and in which colours? Count clockwise from the top dot (top = 0): which number is the blue one, which the pink? I expect blue at 3 o'clock and pink at 6 o'clock, but that is from the code, not a test.
2. Click Rotate trig right once. Which way does the pattern move on the ring (clockwise or counter-clockwise)? Does that match what your SMR does when you send a Rotate Clockwise trigger?
3. Move Morph up and click Rotate trig: does each colour slowly slide from one dot to the next?
4. Move slider 3 to the bottom: its ring dot goes very dim but not out. Move it back up: it brightens.
5. Click Lock 2: its ring dot flashes on and off. Count the flashes in 10 seconds and tell me the number. If you can, lock one channel on your SMR and count its ring LED flashes in 10 seconds too.

## 3. Slider LEDs
1. Each slider's LED gets brighter as you raise the slider. At the top it is dim (25 % of full by design, like the module). Tell me if it is too dark to see.
2. Make the input loud, raise Res (Q) and slider 1: the slider 1 LED should flash fully on and brighter when it clips. Tell me if you see a flash, and which settings made it.

## 4. Env Out lights
1. Fast | Trigger | Slow switch on Slow, then Fast: each light follows the loudness of its band, in its channel colour (blue, teal, green, white, orange, pink).
2. Trigger position: the lights jump on for about a tenth of a second.
3. Pre | Post: with Post, a slider at the bottom keeps its light dark.

## 5. Clip LED and meters
1. (Optional, only if you want to) Turn the track volume up while music plays. When the yellow clip light first comes on, look at the number on the Logic channel meter and tell me it. I expect about -3. If that is confusing, skip it: it stays marked unconfirmed.
2. (Removed: CI already tests that a negative-only input does not light the clip LED, `testDisplay` in tests/plugin_tests.cpp. Nothing for Bryn to do.)
3. The In bars move with the input, the Out bars with the output, the thin line holds the peak for about a second.
4. Stop playback: the bars fall to empty.

## 6. Tell me
1. Which Mac, the run number.
2. Anything that did not match, with the step number.
3. Anything too dark, too bright, too small or hard to see.
