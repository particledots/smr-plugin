# Milestone 7, step 2 test: Lock jack triggers ("Lock odds", "Lock evens")

Test on the Intel Mac first (macOS 12.7.8, Logic 10.7.8). Tell me when you also test on the M4 mac mini. Play noise or music through the plugin the whole time.
Install build run 40641849028 (commit 7ad84ab) as in docs/testing-milestone-6a.md section 1.
After you open the plugin window, wait 10 seconds with audio playing before the first click.

What is new: under each Freq knob there is a button, "Lock odds" on the left and "Lock evens" on the right. It is the SMR's Lock jack: one click is one trigger. The line under the title now also shows "Lock 135" and "Lock 246" (sent/seen).

Start with the switches at 1·3·5 and 2·4·6 (both to the "all channels" side) and all six Lock buttons dark.

## 1. Odds jack
1. Click "Lock odds" once. Lock 1, Lock 3 and Lock 5 light together. Lock 2, 4, 6 stay dark. Does the sound change?
2. Click "Lock odds" again. All three go dark.

## 2. Evens jack
1. Click "Lock evens" once. Lock 2, Lock 4 and Lock 6 light together; 1, 3, 5 stay dark.
2. Click "Lock evens" again. All three go dark.

## 3. The 135 | 1 and 246 | 6 switches
1. Flip the left switch to "1". Click "Lock odds". Only Lock 1 lights. Click again: dark.
2. Flip the right switch to "6". Click "Lock evens". Only Lock 6 lights. Click again: dark.
3. Flip both switches back to the "all channels" side.

## 4. Each channel inverts on its own
1. Tap Lock 3 (it lights).
2. Click "Lock odds". I expect Lock 3 to go dark and Lock 1 and Lock 5 to light.
3. Click "Lock odds" again. I expect Lock 3 to light and Lock 1 and Lock 5 to go dark.
4. Tap Lock 3 to clear it. All six dark.

## 5. Locked channels stay put
1. Click "Lock odds" (1, 3, 5 locked).
2. Click Rotate trig (right) three times. Tell me whether channels 2, 4, 6 move and 1, 3, 5 stay (your ear).
3. Click "Lock odds" again to unlock.

## 6. Fast clicking
1. Look at the "Lock 135" numbers. Click "Lock odds" 6 times quickly. Tell me the numbers (sent/seen) and whether Lock 1, 3, 5 end dark (6 is an even number).

## 7. Save and reopen
1. Click "Lock evens" (2, 4, 6 lit). Save the Logic project, close it, reopen it, open the plugin.
2. Tell me which Lock buttons are lit. They must not have toggled by themselves at load.
3. Click "Lock evens" once: they go dark.

## 8. Logic automation (skip if it takes more than a minute)
1. In the parameter list for the plugin, tell me whether "Lock 1 3 5 (trigger)" and "Lock 2 4 6 (trigger)" are there.

## 9. Tell me
1. Which Mac.
2. Anything that did not match, with the step number.
3. The numbers from step 6.1.
