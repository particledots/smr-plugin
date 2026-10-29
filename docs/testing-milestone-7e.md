# Milestone 7, step 4 test: forbidden notes ("Rotate press" button)

Test on the Intel Mac first (macOS 12.7.8, Logic 10.7.8). Tell me when you also test on the M4 mac mini. Play noise or music through the plugin the whole time.
Install build run 40829856814 (commit bccc09d):
1. Quit Logic.
2. Open https://github.com/particledots/smr-plugin/actions/runs/40829856814 and under Artifacts download Unofficial-SMR-macOS.
3. Unzip both zips (double-click each), open the Unofficial-SMR folder, and double-click Install-Unofficial-SMR.command. If macOS refuses to open it, right-click it, Open, Open. It asks for your Mac password.
4. Open Logic, open a project with an audio track playing noise or music, put Unofficial SMR on it.
After you open the plugin window, wait 10 seconds with audio playing before the first click.

What is new: a button "Rotate press" (dashed outline = plugin only) sits left of the ring, under the left "Rotate trig" button. It is the press of the ROTATE knob. The line under the title now also shows "Rot press" (sent/seen).

How it works on the SMR (manual, "Forbidden Notes", page 16): hold a channel's Lock button and press ROTATE; the note that channel is on becomes forbidden and its light on the ring turns grey. No channel will rotate or spread onto a forbidden note. In the plugin, "hold a Lock" means **shift-click the Lock button** (it stays held); shift-click it again to let go. A normal click on a Lock is a quick tap, so it cannot be combined with a click on "Rotate press".

Start with all six Lock buttons dark, both switches at the "all channels" side, and Rotate trig clicked a few times so the channel lights are on different ring positions.

## 1. Forbid one note
1. Find the ring light of channel 1 (blue). Remember its position.
2. Shift-click Lock 1. Tell me how the button looks while held.
3. Click "Rotate press" once. I expect the blue light at that position to turn grey.
4. Shift-click Lock 1 again to let go. I expect Lock 1 to stay dark (not lit).
5. Click Rotate trig (right) 20 times, one click at a time. I expect channel 1 never to land on the grey position. Tell me whether it ever does.

## 2. Undo straight away
1. Click Rotate trig (right) until a channel 1 light sits on a normal position again.
2. Shift-click Lock 1, click "Rotate press" (grey appears), click "Rotate press" again while Lock 1 is still held. I expect the grey to go and the blue light to come back.
3. Shift-click Lock 1 to let go. Lock 1 stays dark.

## 3. No Lock held
1. With all Locks dark, click "Rotate press" once. I expect nothing to change on the ring.

## 4. Several channels at once
1. Shift-click Lock 2 and Lock 3. Click "Rotate press" once. I expect two grey lights (the positions of channels 2 and 3), and the blue light of channel 1 unchanged.
2. Shift-click Lock 2 and Lock 3 to let go. Both stay dark.

## 5. Clear all
1. Shift-click all six Locks. Click "Rotate press" once. I expect every grey light to go.
2. Shift-click the six Locks to let go. I expect none of them to light.

## 6. The limit of 14 (optional, about 3 minutes)
1. Clear all as in 5.
2. Round 1: shift-click Locks 1 to 5, click "Rotate press", shift-click Locks 1 to 5 to let go. Count the grey lights: expect 5.
3. Click Rotate trig (right) once. Round 2: the same. Expect 10 grey lights.
4. Click Rotate trig (right) once. Round 3: the same. Expect 14 grey lights (the 15th is refused). Tell me the number you count.

## 7. The counter
1. Look at "Rot press" in the line under the title. Click "Rotate press" 4 times, one click at a time. Tell me the numbers (sent/seen). They should be equal.

## 8. Save and reopen
1. Forbid one note (section 1, steps 1 to 4). Save the Logic project, close it, reopen it, open the plugin.
2. Tell me whether the grey light is still there. (The SMR keeps forbidden notes only in a settings slot, which the plugin does not have; the plugin keeps them in the project. See docs/open-questions.md M7-k.)
3. Clear all as in 5.

## 9. Logic automation (skip if it takes more than a minute)
1. In the parameter list for the plugin, tell me whether "Rotate Press (trigger)" is there.
