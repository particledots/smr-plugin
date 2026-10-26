# Capture sheet: recording your real SMR (Milestone 3, round 1)

Machine: the M4 mac mini (Tahoe, Logic 11.2) with the Focusrite Scarlett 2i2 (3rd Gen). Plan: 7 recordings (8 with the optional loopback), about 10 minutes of audio in total plus setup. I have not timed it.
Nothing here is confirmed until the recordings have been analysed. Where I have not checked something, it says so.

## What you need
- The two test files from Claude: `smr_sig_sweep.wav` and `smr_sig_main.wav`. Both are 96 kHz, 24-bit, stereo. The left channel holds the signal and the right channel is silent.
- Three 3.5 mm to 1/4" mono cables.
- Optional: one 1/4" mono (guitar) cable, for the loopback pass P5.
- A phone to take two panel photos.

Check the files are the right ones: `shasum -a 256 ~/Downloads/smr_sig_*.wav` in Terminal (Applications, Utilities, Terminal) should print:
- `smr_sig_sweep.wav` f7bfa5dfca205e67bd94c0acc1f9e8444d7054d128282b8de483e192f326b16c
- `smr_sig_main.wav` ef2d4b4e3b7fdf47df66bae4231ab30214a5ef97587fa91c0c5c5312ebdd55a3

## A. Set up the Scarlett (once)
1. Power on the SMR. It loads the last settings it saved. Do not change the bank or scale.
2. Plug the Scarlett into the M4 over USB.
3. On the Scarlett front: turn both GAIN knobs fully counter-clockwise (minimum). Set both INST/LINE switches to LINE (not lit). Turn AIR off. Turn DIRECT MONITOR off.
4. Turn the big MONITOR knob fully clockwise. I believe this knob sets the level of line outputs 1 and 2; I have not confirmed that. If it does not seem to do anything, tell me.
5. If you have speakers or headphones on the Scarlett, turn them down or disconnect them. Output 1 carries test noise.

## B. Patch the SMR (once)
Unpatch every cable from the SMR first. Then:
- Scarlett rear LINE OUTPUT 1 to the SMR **IN, odds** jack (the one with the black ring, top left of the IN group).
- SMR **OUT, odds** jack to Scarlett front input 1.
- SMR **OUT, evens** jack (the one with the black ring) to Scarlett front input 2.
- Nothing else patched. The SMR IN evens jack stays empty. It is wired to follow the odds jack (manual, Controls and Jacks).

## C. Set the SMR panel (before every pass unless a pass says otherwise)
This follows the manual's "Getting started: your first patch" (page 4).
- All six Lock buttons off (press any lit button once).
- Scale Rotation switch off (down).
- Both FREQ NUDGE knobs fully counter-clockwise (0%).
- SPREAD fully counter-clockwise (0%).
- MORPH fully counter-clockwise (0%).
- Do not hold any Lock button while turning RES (Q). That would lock Q on that channel.
- Do not hold the ROTATE knob down. Holding it for 5 seconds saves settings, and we don't want that. A tap is fine.
- CV Slew, Pre/Post, Fast/Slow, 135/1 and 246/6 switches: leave them where they are. They only matter with cables patched, and none are.

Slider positions are set per pass below. "Up" means pushed all the way to the top.

Take **photo 1** now: the whole panel, light ring and sliders clearly visible, in Rotate mode. Then take **photo 1b** in Scale mode: tap the ROTATE knob once (do not turn it, do not hold it), photograph the ring (bottom six lights show each channel's bank, top arc shows scales), then tap ROTATE once more to return to Rotate mode before recording. Do not change the bank or scale yourself. Photos are a cross-check only: the bank, scale and notes are worked out from the recorded frequencies.

## D. Set up Logic (once)
I have not checked Logic 11.2's exact menu names. If something below doesn't match what you see, tell me what you see.
1. Open Logic. Create an empty project with no tracks.
2. Logic menu, Settings, Audio: set the Input Device and Output Device to "Scarlett 2i2 USB".
3. File, Project Settings, Audio: set Sample Rate to 96 kHz. Bit depth 24.
4. Make two mono audio tracks. Set one to Input 1 and the other to Input 2. Turn input monitoring off on both. Record-enable both.
5. File, Import, Audio File: import `smr_sig_main.wav`, placed at bar 1, on a new stereo audio track. Leave its fader at 0 dB and pan centred. Turn Flex and Follow Tempo off on it if they are on.
6. Confirm the stereo track's output is "Output 1-2" and the master fader is at 0 dB, with no plugins on any track.
For the sweep passes you will import `smr_sig_sweep.wav` in the same way, on its own track, then mute the main one.

## E. The recording passes
For each pass: set the panel, press Record from the very start of the project, wait for playback to finish and the tail to run out, press Stop. Do not trim or cut anything.

Stop times (file length plus about 10 s for ringing to die away; the 10 s is a guess, longer is harmless): P0 40 s; P1 and P2 (sweep, 38 s) about 48 s; P3a, P3b, P3c, P4 and P5 (main, 105 s) about 115 s. Then export the two recorded regions (section F). Use the file names exactly as shown.

Q positions: **Q min** = RES (Q) fully counter-clockwise. **Q mid** = pointing straight up. **Q max** = fully clockwise.

| Pass | SMR sliders | RES (Q) | Play | Names |
|---|---|---|---|---|
| P0 | all six up | Q min | nothing (record 40 s with the Scarlett output 1 cable unplugged from the SMR) | `P0_noise_Qmin_in1_oddsOUT.wav`, `P0_noise_Qmin_in2_evensOUT.wav` |
| P1 | slider 1 up, 2 to 6 down | Q mid | `smr_sig_sweep.wav` | `P1_sweep_s1_Qmid_in1_oddsOUT.wav`, `..._in2_evensOUT.wav` |
| P2 | slider 2 up, others down | Q mid | `smr_sig_sweep.wav` | `P2_sweep_s2_Qmid_...` (same pattern) |
| P3a | all six up | Q min | `smr_sig_main.wav` | `P3a_main_Qmin_...` |
| P3b | all six up | Q mid | `smr_sig_main.wav` | `P3b_main_Qmid_...` |
| P3c | all six up | Q max | `smr_sig_main.wav` | `P3c_main_Qmax_...` |
| P4 | all six up | Q max | `smr_sig_main.wav` | `P4_main_rot10_Qmax_...` |

**Before P0:** unplug the Scarlett output 1 cable from the SMR IN odds jack. Replug it before P1.

**Before P4:** make sure you are in Rotate mode (if the top half of the light ring has one blinking light, you are in Scale mode: tap the ROTATE knob once to go back; manual, Rotation section), then turn the ROTATE knob exactly 10 clicks clockwise. With MORPH at 0% the change is instant. Take **photo 2** of the panel. Do P4. To go back afterwards, turn the knob 10 clicks counter-clockwise. If you are unsure which mode you are in, take a photo and ask me before continuing.

**P5 (optional, loopback, about 5 minutes):** unplug all SMR cables. Connect the guitar cable from Scarlett output 1 to Scarlett input 1. Play `smr_sig_main.wav` and record input 1 only. Name: `P5_loopback_in1.wav`. It lets me separate the Scarlett's own gain from the SMR's.

## F. Export from Logic
Exact menus may differ. For each pass, select the two recorded regions (input 1 and input 2) and use File, Export, Region as Audio File. Choose WAV, 24-bit, 96 kHz, no normalising. If Logic offers to export "original" versus processed, choose original. Save into a folder `SMR-capture-1` on your Desktop. If an option is missing, tell me what you see before going on.

## G. Upload (after all passes)
1. Zip the `SMR-capture-1` folder (right-click, Compress).
2. Go to https://github.com/particledots/smr-plugin/releases and click "Draft a new release".
3. Click "Choose a tag", type `capture-1`, then "Create new tag: capture-1 on publish".
4. Release title: `Hardware capture 1`. Tick "Set as a pre-release".
5. Drag the zip file into the "Attach binaries" box. Wait for the upload bar to finish.
6. Click "Publish release".
The release is public. The files contain only test signals and the SMR's response, no music.
7. Send the two panel photos to me in the chat.

## What I'll analyse (for your information)
Tier A in `docs/status.md`: which channel leaves by which jack (M1-m), noise-source level and shape (M1-n), centre frequencies per channel, resonator decay at three Q settings, level linearity from the stepped noise, and the 96,000 versus about 95,982 Hz question (M2-b, from the P4 rings). Absolute level (M1-n) is the least certain result. The Scarlett figures assume balanced outputs and inputs; with mono TS cables the real level could differ by up to 6 dB (my assumption, not checked). P5 narrows that down.
