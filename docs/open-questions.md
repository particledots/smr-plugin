# Open questions

Items for Bryn to decide. Nothing here has been decided silently. Each entry says
what was read, and where.

## Licensing (found in Milestone 0)

### Q1. JUCE licence route
`LICENSE.md` of JUCE 8.0.6 says the modules are dual-licensed under AGPLv3 or the
JUCE 8 End User Licence Agreement, and that using JUCE without the AGPLv3 route
binds you to the EULA.

Tier figures below come from a JUCE forum post about the JUCE 8 licence amendments
(forum.juce.com, "Amendments to the JUCE End User Licence Agreement for JUCE 8"),
not from the EULA text itself. Check juce.com before relying on them.
- Starter: free, revenue/funding up to $20,000 per year; the forum post says the
  splash screen requirement was removed for Starter.
- Indie: up to $300,000, $40/month or $800 perpetual.
- Pro: no limit.
- For individuals the limit counts total revenue from all sources including
  donations, sponsorship and advertising (same post).

Options for a free plugin:
- A. Starter EULA route: source can stay private, binaries free to share, as long as
  you stay inside the Starter limits and its terms.
- B. AGPLv3 route: public source for the whole plugin (src/ included) under AGPLv3
  whenever binaries are shared. The MIT firmware code can be combined with it.

Decision needed before any public release. Test builds for yourself do not need
this decided.

### Q2. Licence for the code in `src/`
No licence is assigned. Default is all rights reserved. It depends on Q1
(route B forces AGPLv3 on the combined plugin).

### Q3. Kickoff pack says "4ms copyright"; the source says otherwise
The kickoff pack says to copy the MIT licence "with the 4ms copyright".
`third_party/SMR/LICENSE` reads "Copyright (c) 2015 Dan Green (danngreen1@gmail.com)".
No company name appears in it. The notices copy it exactly as written.

### Q4. Using the text "4ms" in repo files
`README.md` of the firmware repo (Guidelines for derivative works) forbids the text
"4ms" and the logo on derivative works, and names faceplates, enclosures and
front-panels. The submodule URL (`.gitmodules`) and the commit provenance in
repo docs unavoidably contain it. Current choice, change if you disagree:
"4ms" is kept out of the plugin name, UI, plugin metadata and the shipped
`THIRD-PARTY-LICENSES.txt`; it may appear in repo-internal files.

### Q5. Module panel look vs hardware licence
The firmware `LICENSE` says the hardware is CC BY-NC-SA. The GUI is meant to
resemble the module (Milestone 6). Whether copying the panel layout counts as
using the hardware design is not something I can settle. Needs your decision or
the designer's answer before the GUI is shared.

## For the Milestone 1 source audit (read now, not yet audited)

### Q6. GPLv3 code in the firmware tree
`third_party/SMR/rotary.c` and `rotary.h` carry "Copyright 2011 Ben Buxton.
Licenced under the GNU GPL Version 3." This is the rotary encoder decoder, a
hardware-only file. Rule for the port: it must not enter `src/core`.

### Q7. ARM CMSIS headers
`filter.c` includes `arm_math.h` (and `audio_util.h` includes `<arm_math.h>`). The
copy in `third_party/SMR/stm32/core/include/arm_math.h` is headed "Copyright (C)
2010 ARM Limited. All rights reserved", CMSIS DSP Library V1.0.10, "Target
Processor: Cortex-M4/Cortex-M3/Cortex-M0". I have not read that licence's terms.
The plan to replace ARM math with a shim (Milestone 2) avoids copying it;
Milestone 1 must read the terms and list every `arm_` call actually used.

### Q8. Dependency on stmlib
The firmware README says to clone a separate `stmlib` repo next to SMR. A grep of
the `.c`, `.h` and `Makefile` files in the zip for the string "stmlib" returned
no matches. So the audit has to confirm whether any stmlib code is actually
needed. Not assumed either way.

## Carried over from the kickoff pack (Milestone 1 settles these)

- Fast | Slow switch Trigger position and the v5 1V/octave mode.
- Rotate encoder press and bank selection.
- Odds/evens normalling and mono input handling.
- Which v5 features are in scope for v0.1.
- Your SMR firmware: "Version 5" is recorded; 5.0 vs 5.1.x unknown. The firmware
  CHANGELOG says 5.1 changed Freq jack slew and Q-lock loading, and 5.1.1 fixed
  Env Out 3 and 4. The port follows 5.1.1.
- Freq as MIDI note, plain pitch parameter, or both.

## Milestone 0 decisions I made that you may want to change

- AU type is a plain **effect** (`aufx`), no MIDI input. The Beads plugin uses a
  MIDI-controlled-effect type to get MIDI. Because Freq-from-MIDI is undecided,
  the type may change at Milestone 4, which changes how Logic lists the plugin.
- Plugin codes: manufacturer `Ptdt` (same as Beads), plugin `Smru`.
- Mono-to-mono and stereo-to-stereo only in Milestone 0.
- Deployment target macOS 12.0 (Beads uses 11.0).
- CI runner `macos-15`, not Beads' `macos-14`: GitHub retires `macos-14` on
  2 November 2026 and fails jobs in scheduled brownouts before then, including
  26 Oct 14:00-24:00 UTC and 29 and 30 Oct (github.blog changelog 2026-10-01).
