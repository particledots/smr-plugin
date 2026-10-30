# Unofficial SMR

A faithful Audio Unit (AU) and VST3 port of the open-source Spectral Multiband Resonator
firmware (v5.1.1, commit `79bc953`), for Logic Pro on macOS. Phase 1 is exact fidelity to the
firmware: no enhancements.

This is a community project. It is not an official product and it is not endorsed by the
original designer or manufacturer. Do not contact them about it. Problems go to the issues
page of this repository.

## Status: pre-release (v0.1 not yet tagged)

- Builds run on GitHub Actions only. The latest runs are green: core tests, parameter tests,
  101 plugin tests (arm64 and x86_64), a comparison of the port's logic against the unmodified
  firmware code, and `auval` (now a hard gate).
- Loading in Logic was confirmed on an Intel Mac (macOS 12.7.8, Logic Pro 10.7.8) and an M4 Mac
  mini (macOS Tahoe, Logic Pro 11.2) for the first pass-through build. Later features were
  checked mainly on the Intel Mac. The full pass on the M4 Mac mini is still to do.
- The VST3 builds and passes the CI checks, but has not been tried in any host. Logic loads AU only.
- Details, test results and open decisions: `docs/status.md` and `docs/open-questions.md`.

## What it is

- Effect plugin, name "Unofficial SMR", manufacturer "particledots".
- Channel layouts: mono in to mono or stereo out, stereo in to stereo out.
- Host sample rates: tested at 44.1, 48, 96 and 192 kHz. The firmware runs at about 96 kHz, so
  the plugin resamples. Other rates may be passed through unchanged; the window says so.
- The window follows the module's panel: six level sliders, Lock buttons, knobs, switches, the
  LED ring and the Env Out lights. Jacks and combos become on-screen buttons and parameters.

## What it is not (yet)

- No System Mode, calibration or settings slots. User-scale editing is not included.
- Env Out is shown as lights only; there is no CV output.
- The Noise switch uses a stand-in generator. The hardware's own noise source and its level are
  not known (`docs/open-questions.md`, M1-n and M7-e).

## Known differences from the hardware

Compared with one recording of a real SMR (`docs/status.md`, Milestone 3 and 5):

- Centre frequencies: 18 of 18 inside 1 cent at the recorded state.
- Peak widths: 6 of 18 inside tolerance. Peak gain: 4 of 18. The knob positions of the recording
  are estimates.
- High frequencies: the port is brighter than the recording by about 6.7 dB at 16 kHz (Q min).
  This is probably the hardware's audio converter, which the firmware code does not contain.
  The cause is not verified and it is not emulated (`docs/open-questions.md`, M3-i).
- Output channels follow the firmware source. The recording showed the first two sliders on the
  opposite output jacks to the source. This is not explained (`docs/open-questions.md`, M1-m, M3-a).
- Lock hold time: the manual says 2 seconds; the source value depends on the firmware's
  main-loop rate, which is a placeholder here (M7-a).

## Install (test builds)

v0.1 is not released yet. Until it is, builds come from GitHub Actions. This installs the AU for Logic.

1. Open https://github.com/particledots/smr-plugin/actions and sign in to GitHub.
2. Click the newest run of "Build plugin (macOS)" with a green tick. If it is red, install nothing.
3. Under Artifacts at the bottom, click `Unofficial-SMR-macOS`. A zip lands in Downloads.
4. Unzip it, then unzip the zip inside it (double-click each).
5. In the `Unofficial-SMR` folder, double-click `Install-Unofficial-SMR.command`. If macOS
   refuses to open it, right-click it, choose Open, then Open. Enter your Mac password when asked.
6. Open Logic Pro. In the Plug-in Manager choose Reset & Rescan Selection. "Unofficial SMR" is
   listed under "particledots".

The Terminal route, which also installs the VST3, is in `docs/INSTALL.txt` (also inside the download).
The build is ad-hoc signed, not notarised by Apple; the installer clears the quarantine flag.

To remove the AU:

    sudo rm -rf "/Library/Audio/Plug-Ins/Components/Unofficial SMR.component"

## Layout

| Path | What |
|---|---|
| `third_party/SMR` | firmware, git submodule pinned to `79bc953bb4b9cf47614d317b4ba9fc9f615c43ea` |
| `src/core` | the ported DSP. No plugin or JUCE code |
| `src/host` | resampler and block adapter. No JUCE code |
| `src/plugin` | JUCE wrapper: parameters, window, host glue |
| `tools/` | offline renderer, comparison scripts, installer |
| `tests/` | headless tests |
| `docs/` | status, open questions, architecture, test sheets |
| `licenses/` | the firmware's licence, verbatim |

## Licences

- **Unofficial SMR as a whole is licensed under the GNU Affero General Public Licence version 3**
  (`LICENSE`). Copyright (c) 2026 particledots. The source for any build is this repository.
- **Firmware-derived code** (the ported DSP in `src/core`, the coefficient tables) comes from the
  Spectral Multiband Resonator firmware, MIT licence, copyright (c) 2015 Dan Green; some files
  also name Hugo Paris. The MIT notice is kept in `THIRD-PARTY-LICENSES.txt` and
  `licenses/SMR-LICENSE.txt` and ships with every build. MIT code can be combined into an AGPL work.
- **JUCE 8.0.6** is used under its AGPLv3 option. JUCE bundles other components (for example the
  VST3 SDK and the Audio Unit SDK) under their own licences, listed in JUCE's `LICENSE.md`.
- **Hardware.** The firmware's licence file says the SMR hardware design is under a Creative
  Commons BY-NC-SA licence. No hardware design files are used. The window's layout follows the
  module's panel; this plugin is free and non-commercial, and uses no logo or maker name. That is
  a statement of intent, not legal clearance.
- The names "SMR" and "Spectral Multiband Resonator" are used as the firmware's guidelines for
  derivative works allow. Nothing here is an official product.
