# Milestone 7, step 3a test: fine tune and transposition (audio only)

No new build. Use the build you already have installed (run 40641849028, commit 7ad84ab). The audio code is the M2 port (`src/core/Params.cpp param_read_freq`, `coarseLadder()`), and the Lock hold buttons (M7 step 1) supply `lock_pressed`. What is NOT in the plugin yet: the Env Out lights that show fine tune and transposition, and the Lock button flashing while a clear is staged. Step 3b adds them after this test. So today you judge by ear (or with a tuner).

Test on the Intel Mac first. Tell me when you also test on the M4 mac mini.

## What the firmware does (read from `params.c param_read_freq`, manual p.11-12)
- Fine tune: Freq Nudge at 0% = no change, 100% = ratio 1.0745 (about +124 cents; manual says about 120, open-questions M1-d). Halfway is about +63 cents (derived from `1 + t/55000`).
- Transposition: hold a channel's Lock button and move that side's Freq Nudge (odds knob for 1, 3, 5; evens knob for 2, 4, 6). The knob is split into 13 equal zones, about 7.7% of travel each: zone 0 (0 to 7.7%) is -6 semitones, zone 6 (46.2 to 53.8%) is 0, zone 12 (92.3 to 100%) is +6.
- Nothing changes until the knob crosses the zone the channel was last set to (0 if never set). Then it follows the knob in both directions.
- While Lock is held, Nudge does transposition only (fine tune is held off). Releasing the Lock after you moved Nudge does not toggle the lock.
- Fine tune and transposition work in the MaxQ filter type (the default). In BpRe they do not (`param_read_freq` else branch). Keep the filter type at the default for this test.

## 0. Setup
1. Start state: switches at 1.3.5 and 2.4.6, all six Lock buttons dark, filter type at default. Play noise or music through the plugin.
2. Drag both Freq Nudge knobs fully up, then fully down. (The firmware ignores a Nudge knob until it has moved more than about 20% from where it loaded; this wakes both.)
3. To hear one channel: raise slider 1 and lower sliders 2 to 6. Q fairly high. A tuner after the plugin helps (the Logic Tuner plugin or any tuner). If the tuner will not lock, judge by ear and say so.

## 1. Fine tune (no Lock held)
1. Freq Nudge odds at 0%. Note the pitch (or the tuner's cents reading).
2. Nudge odds to 50%, then 100%. Expect the pitch to rise smoothly to a bit more than a semitone. Tell me the cents (tuner) or "about a semitone / more / less" (ear), at 50% and at 100%, relative to step 1.

## 2. Transposition of one channel
1. Nudge odds at 0%. Shift-click Lock 1 (it stays held).
2. Drag Nudge odds slowly up to 100%. Expect no pitch change until the middle (about 46 to 54%), then one semitone per step, +6 at the top. Tell me: at what knob position the first step happened, how many steps you counted up to 100%, and the tuner reading at the top (expect about +600 cents).
3. Still holding, drag Nudge odds slowly down to 0%. Expect steps back down through zero to -6 at the bottom. Tell me the tuner reading at the bottom (expect about -600 cents) and whether you heard 12 steps in total.
4. Drag to about 73% (+3). Shift-click Lock 1 to release. Expect: the pitch stays at +3, and Lock 1 does NOT light.
5. With no Lock held, move Nudge odds to 0%. Expect channel 1 to stay at +3 (about 300 cents above its untransposed pitch). Tell me yes or no.

## 3. Check without changing
1. Channel 1 is at +3 and Nudge odds is at about 20%. Shift-click Lock 1 and wiggle Nudge odds a little around 20%. Expect: no pitch change (the knob is not in the zone it was set to).
2. Drag Nudge odds up slowly. Expect nothing until the knob reaches the +3 zone (about 69 to 77%); from there it follows again (above 77% gives +4 and up). Tell me where the pitch started to follow.
3. Return to about 73%, shift-click Lock 1 to release.

## 4. Several channels at once
1. Shift-click Locks 1, 3 and 5. Drag Nudge odds up to about 90% (+5). Expect channels 1, 3, 5 to move together, 2, 4, 6 not at all (raise sliders 1 to 6 to hear all; use the tuner on one channel if you can).
2. Release Locks 1, 3, 5 (shift-click each). Expect no lock lit.
3. Repeat with Locks 2, 4, 6 and Nudge evens. Tell me if the evens move and the odds do not.

## 5. A locked channel
1. Tap Lock 5 (lit, locked). Shift-click Lock 5 and drag Nudge odds across the middle to about 90%. Expect channel 5 to transpose even though it is locked. Shift-click Lock 5 to release. Lock 5 must stay lit.

## 6. Clear all
1. Set channel 1 to +3 and channel 2 to about -2 (use Locks 1 and 2 with Nudge odds and Nudge evens), unlock everything.
2. Shift-click all six Lock buttons. Count 2 seconds, then shift-click all six to release. Tell me: did the transpositions clear, and did any Lock button light? (Source: it needs 36000 main-loop passes, about 3 s at the 12 kHz placeholder; manual says 2 s; open-questions M1-c. If any Lock lit, click it dark.)
3. Set channel 1 to +3 again. Shift-click all six Locks and hold about 5 seconds. Release Lock 1 first, wait 2 seconds, listen. Expect channel 1 back to untransposed, no Lock lit. Release the other five.
4. Locked channel: set channel 5 to +3, tap Lock 5 (locked). Shift-click all six, hold 5 seconds, release all. Expect channel 5 to keep its pitch. Then tap Lock 5 (unlock). Expect its pitch to drop to untransposed.

## 7. Tell me
1. Which Mac and the run number.
2. For each step: matches, or what you heard (with the step number).
3. The numbers asked for: cents at 1.2; first-step position and step counts at 2.2 and 2.3; where it followed again at 3.2; and the result of 6.2.
