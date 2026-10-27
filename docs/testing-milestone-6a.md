# Milestone 6a test: the plugin window (layout and controls)

Test on the M4 mac mini first. If it works there, repeat on the Intel Mac. Same steps both times.
No hardware, no files to send. Tell me what you see.

## 1. Install the newest build
1. Quit Logic.
2. Open https://github.com/particledots/smr-plugin/actions and open the newest run called "Build plugin (macOS)" that has a green tick. Write down its number (the number in the web address).
3. Under Artifacts, download Unofficial-SMR-macOS. Unzip both zips (double-click each), open the Unofficial-SMR folder, and double-click Install-Unofficial-SMR.command.
4. Open Logic, open a project with an audio track playing noise or music, put Unofficial SMR on it.

## 2. Look
1. The window opens at about 900 x 742. It should show six vertical sliders with round Lock buttons above them, knobs left and right, a big knob in the middle, and a row of dashed boxes along the bottom. There is no ring and no meters yet; that is 6b.
2. Top right shows the line "Host ... Hz, latency ... samples, underruns ...". Write it down.
3. Drag the bottom-right corner of the window larger and smaller. Everything should scale together. Tell me the smallest and largest size Logic allows.

## 3. Controls (play audio the whole time)
For each, say what happened. A one-word answer is fine.
1. Drag each of the six sliders. The sound for that band gets louder and quieter. The line at the bottom of the window shows its value while you drag.
2. Double-click a slider. It should jump to its top.
3. Click each Lock button. It lights up. Turn Rotate (step 5) with one locked: that channel should not move.
4. Turn Res (Q), Morph, Spread, both Freq nudge knobs and both Freq knobs. Do they change the sound? Does double-click reset them?
5. Click "Rotate trig" on the left once, then on the right once. Then click each ten times quickly. Tell me if any click seems to do nothing.
6. Turn the big middle knob slowly clockwise, then counter-clockwise, then use the mouse wheel over it. Tell me if the sound steps like the Rotate trig buttons.
7. Click each small switch: 1.3.5 | 1, 2.4.6 | 6, CV slew, Scale rotation, Pre | Post, and the three-position Fast | Trigger | Slow (click left, middle, right).
8. Open the Bank box and pick another bank. Open Two-Pass and One-Pass. Open the Filter Type box.
9. Click Scale (down triangle) and Scale (up triangle) a few times.

## 4. Save and reopen
1. Move three sliders, one knob and two switches. Save the project, close it, reopen it, open the plugin window. The controls should be where you left them.

## 5. Tell me
1. Which Mac, and the run number from step 1.2.
2. The line from step 2.2.
3. Anything that did not match, with the step number.
4. Anything hard to read or hard to hit with the mouse.
