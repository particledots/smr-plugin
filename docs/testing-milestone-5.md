# Milestone 5 test: the plugin in Logic (round 1: M4 mac mini only, 2 files)

No SMR, no cables, no microphone. You play a test file through the plugin inside Logic and export (bounce) the result twice. CI then checks that Logic gives the same samples as the plugin run in CI.
Round 1 is the M4 mac mini at 96 kHz only. If it works, round 2 repeats it on the Intel Mac and at 48 kHz.
I have not checked Logic 11.2's menu names. If something does not match, stop and tell me what you see.

## 1. Install the newest build (M4 mac mini)

1. Open https://github.com/particledots/smr-plugin/actions and click the newest run called "Build plugin (macOS)". It must have a green tick.
2. Scroll down to **Artifacts** and click **Unofficial-SMR-macOS**. Install it with the steps in INSTALL.txt (inside the download). Quit Logic first. Before installing, run: `rm -rf ~/Library/Audio/Plug-Ins/Components/"Unofficial SMR.component"`. After installing, run: `killall -9 AudioComponentRegistrar`.

## 2. Download the test file

1. Open https://github.com/particledots/smr-plugin/raw/m5-inputs/M5_in_96000.wav (it downloads by itself; no sign-in needed). Size about 8.6 MB.
2. (Optional check) Terminal: `shasum -a 256 ~/Downloads/M5_in_96000.wav` should start with `79dea9807ed5a918`.

## 3. Make the two recordings

1. Logic: new empty project, no tracks. File, Project Settings, Audio: Sample Rate 96 kHz, 24 bit.
2. File, Import, Audio File: choose `M5_in_96000.wav`, place at bar 1. One mono track appears. Fader 0 dB, pan centred. Flex and Follow Tempo off. Cycle off.
3. Insert the plugin on that track: Audio FX slot, Audio Units, particledots, Unofficial SMR. If Logic offers a choice, pick "Mono -> Stereo", else "Stereo". Change no control.
4. Write down the line of text at the top of the plugin window.
5. File, Bounce, Project or Section. Settings: PCM, WAV, 24 bit, 96000, Normalize off, Dithering none, offline, Add to project off. File name: `M5_M4mini_96000_defaults`. Save to a new folder `M5-bounces` on the Desktop.
6. In the plugin window drag "Res (Q) Knob" fully to the right. Change nothing else. Bounce again, same settings, file name `M5_M4mini_96000_Qmax`, same folder.

## 4. Send them

1. Right-click the folder `M5-bounces`, Compress.
2. Go to https://github.com/particledots/smr-plugin/releases, "Draft a new release". Tag: `m5-logic-1` (choose "Create new tag on publish"). Title: `Logic bounces 1`. Tick pre-release. Drag the zip in, wait for the bar, Publish.
3. Tell me in the chat: the text from step 3.4, any menu that differed, anything odd you heard.
