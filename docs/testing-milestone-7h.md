# Milestone 7, step 6 test: Noise switch (M7-e)

Test on the Intel Mac first (macOS 12.7.8, Logic 10.7.8). Tell me when you also test on the M4 mac mini.
Install build run 40987029109 (commit a683e44):
1. Quit Logic.
2. Open https://github.com/particledots/smr-plugin/actions/runs/40987029109 and under Artifacts download Unofficial-SMR-macOS.
3. Unzip both zips (double-click each), open the Unofficial-SMR folder, and double-click Install-Unofficial-SMR.command. If macOS refuses to open it, right-click it, Open, Open. It asks for your Mac password.
4. Open Logic, open a project, add an audio track (or use one with a region) and put Unofficial SMR on it.
After you open the plugin window, wait 10 seconds before the first click.

What is new: a switch "Noise (stand-in)" under "Scale rotation" on the left, with a dashed outline (plugin only). Off = the track's audio goes in, as before. On = the track's audio is replaced by a noise generator on both inputs, like an SMR with nothing patched into IN. The noise is my stand-in: white noise, not the SMR's own noise, so it will not sound the same as your SMR. I have not seen the window; the switch position is a guess.

## 1. The switch
1. Find the switch. Is it visible, inside the panel, not on top of another label or control? Tell me where it sits.
2. Click it on and off. I expect the dot to go where you click, and the dashed outline to show it is plugin only.

## 2. Noise on, nothing playing
1. Stop playback (no audio into the plugin) and turn Noise on.
2. I expect sound at once, with the transport stopped or running, and the six lights under the sliders to move. Tell me: what does it sound like (a drone of the ring's pitches)? Is it quiet, medium or loud? Is it steady, or does it click or stutter?
3. Move Q and the six sliders. I expect the sound to change as on your SMR. Tell me what is different from your SMR with nothing patched in.
4. Rotate CW (right): I expect the pitches to move one step each time.
5. Turn Noise off. I expect the sound to ring down and stop.

## 3. Noise on, audio playing
1. Play a region on the track (music or noise). Noise off: I expect to hear the SMR resonate your audio.
2. Turn Noise on while it plays. I expect your audio to stop going in: you hear only the noise resonating, and the region has no effect on the sound.
3. Turn Noise off. I expect your audio back.
4. Tell me what you hear in each case.

## 4. Meters and clip
1. With Noise on and the region playing, look at the input meters. I expect them to show the noise (a steady low level, about -20 dBFS rms, peaks near -15 dBFS), not your region.
2. Does the clip LED stay dark? I expect it to.

## 5. Save and reopen
1. Turn Noise on, save the Logic project, close it, reopen it.
2. I expect Noise to be on, and the drone to start when you play. Tell me what happens.
3. Turn it off, save, close, reopen. I expect it to be off.

## 6. Automation and new instance
1. In the Logic automation list for the plugin, is "Noise (stand-in)" there?
2. Add a second Unofficial SMR on another track. I expect Noise to be off there.

## Report
Tell me per section what happened. For anything different from "I expect", say what you saw or heard. Say which Mac and which build run you installed.
