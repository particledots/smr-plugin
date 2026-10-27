# Milestone 5 test: the plugin in Logic against the plugin in CI

Goal: show that Logic hosting the plugin gives the same samples as the plugin run headless in CI (same code, same input, same settings).
Fidelity against the real SMR is NOT tested here (see "What this does not test" at the end).
Nothing is confirmed until CI has compared your recordings and I have reported the numbers.

You need 8 recordings: 2 Macs x 2 sample rates (96 kHz, 48 kHz) x 2 settings (defaults, Q at 100%).
About 10 minutes per Mac. I have not checked the menu names of Logic 10.7.8 or 11.2. If a menu does not match, stop and tell me what you see.

## 1. Install the build (each Mac)

1. Open https://github.com/particledots/smr-plugin/actions (signed in).
2. Click the newest run of "Build plugin (macOS)". It must be green. Under **Artifacts** click **Unofficial-SMR-macOS**.
3. Quit Logic. In Terminal run: `rm -rf ~/Library/Audio/Plug-Ins/Components/"Unofficial SMR.component"`
4. Install with the steps in INSTALL.txt (inside the download), then run `killall -9 AudioComponentRegistrar`.

## 2. Get the two input files (each Mac)

1. On the same Actions page click the newest run of "Logic recordings (Milestone 5)". It must be green.
2. Under **Artifacts** click **M5-test-inputs**. Unzip it. It holds `M5_in_96000.wav`, `M5_in_48000.wav` and `SHA256.txt`.
3. Check the files: in Terminal, `cd` to the folder and run `shasum -a 256 M5_in_*.wav`. The two lines must match the lines in `SHA256.txt`.

## 3. Recordings at 96 kHz (each Mac)

1. Logic: create an empty project with no tracks. File, Project Settings, Audio: Sample Rate 96 kHz, bit depth 24.
2. File, Import, Audio File: choose `M5_in_96000.wav`, place it at bar 1. A mono track appears. Fader 0 dB, pan centred. Turn Flex and Follow Tempo off if they are on. Cycle off.
3. On that track insert the plugin: Audio FX slot, Audio Units, particledots, Unofficial SMR. If Logic offers a choice, pick "Mono -> Stereo" (or "Stereo" if that is all there is). Leave every control as it is.
4. Write down the status line text at the top of the plugin window (host rate, latency, underruns).
5. File, Bounce, Project or Section. Settings: PCM, WAV, 24 bit, sample rate 96000, Normalize off, Dithering none, Bounce Mode offline, Add to project off. Name it exactly `M5_M4mini_96000_defaults` (M4 mac mini) or `M5_IntelMac_96000_defaults` (Intel Mac). Save into a folder `M5-bounces` on the Desktop.
6. In the plugin window drag "Res (Q) Knob" fully to the right (100%). Change nothing else. Bounce again, same settings, name `M5_M4mini_96000_Qmax` or `M5_IntelMac_96000_Qmax`.

## 4. Recordings at 48 kHz (each Mac)

Repeat section 3 in a NEW empty project with Sample Rate 48 kHz, importing `M5_in_48000.wav`, and bounce at 48000. Names: `M5_M4mini_48000_defaults`, `M5_M4mini_48000_Qmax` (or `IntelMac`). Plugin freshly inserted, all controls at defaults for the first bounce.

## 5. Send the recordings

1. Put all bounces from both Macs in one folder `M5-bounces` (copy the files from the other Mac over). 8 files.
2. Zip it (right-click, Compress).
3. https://github.com/particledots/smr-plugin/releases, "Draft a new release", tag `m5-logic-1`, title `Logic bounces 1`, tick pre-release, drag the zip in, wait for the bar, Publish.
4. Tell me here: the four status line texts (each Mac, each rate), whether any Logic menu or setting differed from the steps above, and anything you heard that was odd (crackle, silence, loudness).

## What this does not test

- The hardware. The plugin at its default state is not the state your SMR was in for capture-1 (different scales per channel, an odds Freq offset). That state cannot be set from the plugin window (open-questions.md M5-d), so CI renders the plugin at that state instead (docs/status.md, Milestone 5).
- VST3 (Logic loads AU only).
