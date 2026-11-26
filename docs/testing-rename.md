# Test: rename to "Spectral Harmonic Rotator" (Intel Mac, macOS 12.7.8, Logic 10.7.8)

Build run 45888613397 (commit 9fd048f): https://github.com/particledots/smr-plugin/actions/runs/45888613397

1. Before installing: open Logic, open a project that uses Unofficial SMR (or make one: put it on an audio track, set slider 3 to the bottom and turn Morph fully right). Save it as "rename-test". Quit Logic.
2. Open the run link, and under Artifacts download Spectral-Harmonic-Rotator-macOS.
3. Unzip both zips (double-click each). Open the Spectral-Harmonic-Rotator folder and double-click Install-Spectral-Harmonic-Rotator.command (if refused: right-click, Open, Open). Enter your Mac password. Note the last lines. It should say "Installed:" and show no "WARNING".
4. Finder, Go > Go to Folder: /Library/Audio/Plug-Ins/Components . "Spectral Harmonic Rotator.component" is there and "Unofficial SMR.component" is not.
5. Open Logic. Logic Pro > Preferences > Plug-in Manager. Select particledots, then Reset & Rescan Selection. Expect one entry, "Spectral Harmonic Rotator", marked successfully validated, and no "Unofficial SMR".
6. Open "rename-test". Expect: no "plug-in not found" message; the slot shows Spectral Harmonic Rotator; the window title reads SPECTRAL HARMONIC ROTATOR; slider 3 and Morph are as saved; it makes sound on play.
7. Drag the window's corner to its smallest, then its largest. The title stays on one line between the two gold lines and is readable at the smallest size.
8. Put a new Spectral Harmonic Rotator on another track. It opens.
9. Save, close, reopen "rename-test". Both instances keep their settings.
