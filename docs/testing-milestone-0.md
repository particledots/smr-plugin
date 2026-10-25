# Milestone 0 test: empty pass-through

Goal: the plugin loads in Logic on both Macs and passes audio unchanged.
Nothing is confirmed until you run this and report back.

Logic Pro only hosts Audio Units. The VST3 is built and checked in CI (universal, macOS 12.0),
but it cannot be tested in Logic. Only the AU is tested here.

## 1. Get the build

1. Go to https://github.com/particledots/smr-plugin/actions and sign in to GitHub
   (downloading an artifact needs a signed-in browser).
2. Click the newest run of "Build plugin (macOS)". It must show a green tick.
   If it is red, do not install anything. Send me the run's link or a screenshot.
3. Scroll to **Artifacts** at the bottom and click **Unofficial-SMR-macOS**.
   A file `Unofficial-SMR-macOS.zip` lands in Downloads.
4. Do not double-click it. Follow the Terminal steps in `INSTALL.txt` (same text as
   `docs/INSTALL.txt` in the repo). If Safari already unzipped it and the zip is gone,
   tell me what `ls ~/Downloads | grep -i smr` shows and I will give you the matching steps.

Do this on each Mac.

## 2. Check in Logic (both Macs)

A. **Plug-in Manager.** Open it (it lives in the Logic Pro menu under Settings in Logic 11,
   and under Preferences in older versions; if you cannot find it, tell me what the menu shows).
   Click Reset & Rescan Selection or Rescan. Find "Unofficial SMR" under "particledots".
   Write down the status shown (for example "Validation passed" or an error).

B. **Load it.** New audio track. Drop in any music loop. In an Audio FX slot choose
   Audio Units > particledots > Unofficial SMR. Pick the Stereo variant if offered.
   A small dark window should open saying "Unofficial SMR / Milestone 0: empty pass-through".

C. **Passes audio unchanged.**
   1. Play the loop. Click the plugin's bypass on and off. You should hear no difference.
   2. Null test (stronger): duplicate the track so two identical tracks play together.
      On the duplicate, remove the SMR plugin and insert Logic's Gain plugin with Phase Invert on.
      Leave Unofficial SMR on the first track. Play with both unmuted.
      A perfect pass-through cancels to silence: the Stereo Out meter stays at minus infinity.
      Then bypass Unofficial SMR: the sound should come back doubled.

D. **Mono track.** Add a mono audio track, insert Unofficial SMR, confirm it loads and plays.

E. **Save and reopen.** Save the project, quit Logic, reopen it. The plugin should reload
   without a warning.

## 3. Report back

For each Mac (M4 mini Logic 11.2, Intel Mac Logic 10.7.8): A status text, then pass or fail
for B, C1, C2, D, E. For any failure send the exact message or a screenshot.
Also tell me which GitHub run (the number) you installed from.
