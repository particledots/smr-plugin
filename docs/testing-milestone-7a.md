# Milestone 7, step 1 test: Lock buttons (tap, hold, Q lock)

Test on the Intel Mac first (macOS 12.7.8, Logic 10.7.8). Tell me when you also test on the M4 mac mini. Play noise or music through the plugin the whole time.
Install the newest build as in docs/testing-milestone-6a.md section 1. Write down the run number.
After you open the plugin window, wait 10 seconds with audio playing before the first click. (The firmware's Q knob filter starts at zero and settles slowly; a Lock held while it settles would Q-lock by itself. This is firmware behaviour, `params.c param_read_q`.)

## How the Lock button works now
- Plain click: a tap. It toggles the lock on or off.
- Shift-click: a sticky hold. The button stays "pressed" until you shift-click it again. While it is held, its outline flashes.
- Hold plus turning Res (Q): that channel's Q locks. The button flickers once in a while (inverted for a moment, about every 2 seconds).

## 1. Tap
1. Click Lock 1 once, with no shift. The button lights and the sound changes (channel 1 stops following rotation).
2. Click Lock 1 again. The button goes dark.
3. Click Lock 1 five times quickly. Tell me if it ever stays in the wrong state.

## 2. Q lock (needs shift-click)
1. Shift-click Lock 3. Its outline flashes (a second inner ring appears and goes away).
2. Drag the Res (Q) knob about a third of its travel. Tell me whether the sound of channel 3 changes differently from the other channels.
3. Shift-click Lock 3 again to release. The Lock 3 button must NOT turn on (releasing after a Q turn does not toggle the lock).
4. Drag Res (Q) a long way. Channel 3 should keep its Q while the others follow the knob.
5. Watch Lock 3 for 10 seconds. It should flicker once in a while (inverted for an instant). Tell me about how many seconds between flickers.

## 3. Long hold: unlock but keep the Q lock
1. Click Lock 4 (it lights). Shift-click it, turn Res (Q) a little, shift-click it again (Q lock is set; Lock 4 is still lit).
2. Shift-click Lock 4 and do not touch anything. Start counting.
3. Tell me how many seconds pass until Lock 4 goes dark. I expect about 4 seconds (the firmware counts 50000 main-loop passes; the main-loop rate is a placeholder, so the real seconds matter to me).
4. Shift-click Lock 4 again to release. Lock 4 must stay dark, and its flicker (Q lock) must still happen.
5. Click Lock 4 (tap) twice. The first tap turns it on, the second turns it off and clears the Q lock (the flicker stops).

## 4. Lock parameter follows
1. After step 1.1 (Lock 1 lit), in Logic open the plugin's parameter automation list (press A on the track, then the parameter menu) and pick "Lock 1". Tell me the value shown while Lock 1 is lit and while it is dark. Skip this if it takes more than a minute.

## 5. Tell me
1. Which Mac, the run number.
2. Anything that did not match, with the step number.
3. The seconds from step 3.3, and the flicker interval from step 2.5.
