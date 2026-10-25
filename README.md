# Unofficial SMR (work in progress)

A faithful AU / VST3 port of the open-source Spectral Multiband Resonator firmware
(v5.1.1, commit `79bc953`), for Logic Pro. Phase 1 is exact fidelity, no enhancements.
This is a community project. It is not an official product, and it is not endorsed by
the original designer or manufacturer. Do not contact them about it.

Status: **Milestone 0**. The plugin is an empty pass-through. It contains no firmware
code yet, and nothing here is confirmed to load in Logic until a test says so.

## Layout

| Path | What |
|---|---|
| `third_party/SMR` | firmware, git submodule pinned to `79bc953bb4b9cf47614d317b4ba9fc9f615c43ea` |
| `src/core` | the ported DSP. No plugin or JUCE code allowed here |
| `src/plugin` | JUCE wrapper: parameters, GUI, host glue |
| `tools/` | offline renderer and comparison scripts (later milestones) |
| `tests/` | headless tests |
| `docs/` | `open-questions.md`, test instructions, later the architecture notes |
| `licenses/` | verbatim upstream licence |

## Builds

GitHub Actions only (`.github/workflows/build.yml`). Output: AU and VST3, universal
(arm64 + x86_64), deployment target macOS 12.0. Download `Unofficial-SMR-macOS` from the
run's Artifacts. Install and test steps: `docs/testing-milestone-0.md`.

## Licensing

Firmware code: MIT, copyright notices kept in `THIRD-PARTY-LICENSES.txt` and
`licenses/SMR-LICENSE.txt`. Code in `src/` has no licence assigned yet.
JUCE 8.0.6 is used under its own terms (AGPLv3 or the JUCE 8 EULA). Open decisions are in
`docs/open-questions.md`.
