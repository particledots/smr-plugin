# Milestone 7, step 5 test: 1V/oct lights (fourth position on the Fast | Trig | Slow switch)

Test on the Intel Mac first (macOS 12.7.8, Logic 10.7.8). Tell me when you also test on the M4 mac mini. Play noise or music through the plugin unless a step says to stop it.
Install build run 40880607980 (commit e2df742):
1. Quit Logic.
2. Open https://github.com/particledots/smr-plugin/actions/runs/40880607980 and under Artifacts download Unofficial-SMR-macOS.
3. Unzip both zips (double-click each), open the Unofficial-SMR folder, and double-click Install-Unofficial-SMR.command. If macOS refuses to open it, right-click it, Open, Open. It asks for your Mac password.
4. Open Logic, open a project with an audio track playing noise or music, put Unofficial SMR on it.
After you open the plugin window, wait 10 seconds with audio playing before the first click.

What is new: the switch under "Pre · Post" on the right (Fast · Trig · Slow) now has a fourth position, **1V/oct**. It is the SMR's "hold ROTATE while flipping to Fast" (manual page 18). It is plugin only, so it has a dashed outline round the right-hand quarter of the switch. The plugin has no CV jacks, so in this mode only the six Env Out lights show it: a brighter light means a higher pitch, a dimmer light a lower pitch, and a light never goes fully off.

I have not seen the window. The switch is wider and the label longer than before; I do not know if it looks right.

Start with the switch on Slow, both Pre | Post and Morph at their usual settings, and all six Lock buttons dark.

## 1. The switch
1. Click each of the four positions in turn: Fast, Trig, Slow, 1V/oct. I expect the dot to go where you click each time.
2. Tell me: is the label under the switch cut off or overlapping anything? Does the dashed outline sit round the 1V/oct position only?

## 2. The lights in 1V/oct
1. With the switch on Slow, watch the six lights under the sliders: they follow the loudness of each band.
2. Click 1V/oct. Mute the track so there is no sound. I expect the six lights to stay on at a steady brightness, each a different brightness, none of them off, and not following the sound.
3. Tell me the order from dimmest to brightest. I expect the brightness to rise with pitch. At the default settings channels 1 to 4 are 65.4, 73.4, 82.4 and 98.0 Hz (CI log, run 40880607980), so I expect channel 1 dimmest and channel 4 brighter than channel 1. My unit test gives channel 1 about an eighth of full brightness and channel 6 about a quarter, so all of them are dim.
4. Unmute and play sound. I expect the lights not to move with the sound.

## 3. Rotate and the lights
1. Pull sliders 2 to 6 right down, so you hear only channel 1. Unmute the track and play noise.
2. Click Rotate trig (right) ten times, one click at a time. After each click, listen to the pitch of channel 1 (higher, lower, or the same as before) and look at its light (brighter, dimmer, or the same). I expect a higher pitch to give a brighter light and a lower pitch a dimmer one. Tell me the ten results as pairs, for example "up, brighter".
3. Put the sliders back up.

## 4. Transposition
1. Shift-click Lock 1 (it stays held). Nudge odds at 0%. Drag Nudge odds up slowly to the top (+6 semitones). While you turn it the lights show the transposition display, as in the last step.
2. Shift-click Lock 1 to let go. Wait 2 seconds without touching anything. I expect the lights to go back to the 1V/oct lights, with channel 1 brighter than before.
3. Shift-click Lock 1 again, drag Nudge odds back down to 0%, shift-click Lock 1 to let go. I expect channel 1 to return to its first brightness.

## 5. Leaving 1V/oct, and jumping straight across
1. Click Slow. I expect the lights to follow the sound again. Click 1V/oct again, then Trig. I expect the lights to flash with loud hits (Trigger mode), not steady.
2. Click Fast, then click 1V/oct directly (not through the middle). I expect steady 1V/oct lights.
3. Click Fast directly. I expect the lights to follow the sound again.
4. Click Slow, then 1V/oct directly, then Slow directly. Same expectations.
Tell me every case where the lights did not match the switch.

## 6. Glissando (Pre | Post)
1. Click 1V/oct. Turn Morph up to about three quarters. Set Pre | Post to Post.
2. Click Rotate trig (right) once and watch channel 1's light. Manual page 18: in Post the 1V/oct output stays at the first note and then jumps to the second when the morph is finished. I expect the light to hold, then change in one step.
3. Set Pre | Post to Pre and click Rotate trig (right) once. I expect the light to change gradually over the morph time.
Tell me what you saw in each case. (The window redraws these lights 30 times a second; the SMR redraws about 6 times a second, so on the SMR the Pre glide looks a little steppier.)

## 7. Saved project
1. Set 1V/oct. Save the Logic project, close it, reopen it.
2. I expect the plugin to open on 1V/oct with steady lights.

## 8. The sound does not change
1. Play steady noise. Switch between Slow and 1V/oct a few times. I expect no change in the sound.

## 9. On your SMR (optional)
1. Hold the ROTATE button down, flip the Fast | Slow switch to Fast, and let go of ROTATE. I expect the six Env Out lights to go steady, none off, brighter for higher pitches.
2. Turn the ROTATE knob a few clicks. Higher notes should be brighter.
3. With Morph up, compare Pre and Post as in section 6.
4. Flip the switch to the middle or to Slow: the lights should follow the sound again.
Tell me anything that is different from the plugin. The scale, bank and Morph will not match exactly, so I only need to know whether the behaviour agrees.

## 10. One question
Did you ever record automation in Logic on the Fast | Trigger | Slow switch of this plugin? The parameter now has four values, so old automation would be shifted. If you did not, ignore this.

## What to send back
For each section: what you saw, and anything that differs from "I expect". If a section is unclear, skip it and say which.
