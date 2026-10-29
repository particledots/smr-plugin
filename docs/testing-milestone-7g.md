# Milestone 7, step 5b test: label fix, forbidden notes reset on load

Test on the Intel Mac first. Tell me when you also test on the M4 mac mini.
Install the build named in my message (same steps as docs/testing-milestone-7f.md, install the AU only).

## 1. Label
1. Open the plugin window. Look at the label under the right-hand switch: "Fast · Trig · Slow · 1V/oct".
2. Tell me: is the whole label inside the panel border, including the "t"? Resize the window larger and smaller once and check again.
3. Click all four positions once. I expect the dot to go where you click.

## 2. Forbidden notes reset on load
1. Shift-click Lock 1 (it stays held). Click "Rotate press". Shift-click Lock 1 to let go. I expect a grey light on the ring.
2. Save the Logic project, close it, reopen it, open the plugin.
3. I expect no grey light on the ring and the counter at 0.
4. Tell me what you see.

## 3. Nothing else changed
1. Set 1V/oct, save, close, reopen. I expect 1V/oct and steady lights, as before.
