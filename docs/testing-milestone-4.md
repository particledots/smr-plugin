# Milestone 4 test: parameters, saved state, audio

Goal: all 41 controls appear as parameters, can be automated, survive save and reopen,
and the plugin now processes audio (the SMR core is wired in).
Nothing is confirmed until you run this and report back. Fidelity against the real SMR is NOT part of this test.

## 1. Get the build (each Mac)

1. Open https://github.com/particledots/smr-plugin/actions (signed in).
2. Click the newest run of "Build plugin (macOS)". It must be green. If red, do not install; send me the link.
3. Under **Artifacts** click **Unofficial-SMR-macOS**.
4. Install with the Terminal steps in `INSTALL.txt` (as for Milestone 0). Quit Logic first.
   Delete the old copy first: `rm -rf ~/Library/Audio/Plug-Ins/Components/"Unofficial SMR.component"` then install, then `killall -9 AudioComponentRegistrar`.

## 2. Checks in Logic (both Macs)

A. **Loads.** Plug-in Manager: Reset & Rescan Selection. Note the status for Unofficial SMR. Insert it on an audio track with a music loop (Audio FX > Audio Units > particledots > Unofficial SMR, Stereo).
   The window shows a status line (host rate, latency, underruns) above a list of controls.
   Write down the status line text.

B. **Parameter list.** In the plugin window count the controls (sliders, switches, menus). Open the track's Automation (press A), click the parameter menu. Expected: 41 entries, e.g. "Level 1" to "Level 6", "Res (Q) Knob", "Morph", "Spread", "Freq Odds", "Bank", "Lock 1" to "Lock 6", "Filter Mode", "Filter Type". Tell me the count and any name that looks wrong.

C. **Audio is processed.** Play the loop. You should hear the spectral resonator effect (not a clean pass-through). Turn "Level 1" to "Level 6" down and up; move "Morph" and "Res (Q) Knob". Tell me what you hear, including if it is very loud, distorted, or silent. (Level scale M1-n and channel mapping M1-m are placeholders.)
   Watch the Logic CPU meter while playing: note roughly how high it goes (important on the Intel Mac).
   Stop playback: the sound should ring out for a few seconds. Note any crackle or CPU spike during the tail.

D. **Automation.** Set Automation to Latch, press Play, move "Morph" and "Level 1" during playback, stop, switch to Read, play again. The controls should move on their own and the sound follow.

E. **Triggers.** Switch "Rotate CW" on, then off. The resonant pitches should change. Repeat with "Scale Up". (They act when switched on; switch off again before the next use.)

F. **Locks and bank.** Set "Lock 2" on, then "Rotate CW" on/off; channel 2 should stay put. Choose a different "Bank". Tell me if the sound changes.

G. **Save and reopen.** Set some obviously different values: Level 3 at 50%, Morph at 70%, Lock 2 on, Bank to Orange 1, after a couple of Rotate CW triggers, Filter Mode One-Pass. Save, quit Logic, reopen. Check every one of those is back. The sound should resume with the same rotation, not the default.
   Special case (expected, not a bug): if "Freq Nudge" knobs were saved high, that nudge stays silent until you move the knob a good amount (this follows the firmware's power-up sleep, M4-b).

H. **Mono track.** Add a mono audio track with Unofficial SMR. It should load and make sound on both sides if the track is mono-to-stereo, or a mixed mono sound on a mono output.

I. **Latency.** The status line shows a latency in samples. Tell me the number. No need to compensate by ear.

J. **Change buffer size** (Logic Settings > Audio) from 256 to 1024 and back, with the plugin open. It should keep playing without clicks beyond the switch itself and keep its settings.

## 3. Report back

Per Mac (M4 mini Logic 11.2, Intel Mac Logic 10.7.8): status text (A); count (B); C what you hear and CPU peak; pass/fail for D, E, F, G, H, J; latency number (I). For any failure send the exact behaviour or a screenshot, and the Actions run number you installed.
