# Status

## Milestone 0: Foundations. DONE (confirmed 2026-10-25)

Confirmed by CI run 40237397289 (commit 856d513, green):
- third_party/SMR submodule at 79bc953bb4b9cf47614d317b4ba9fc9f615c43ea (checked in CI)
- AU and VST3 universal (x86_64 + arm64), minimum macOS 12.0 on every slice
- auval on the CI runner: AU VALIDATION SUCCEEDED (arm64 runner)
- tests/core_test.cpp (bit-exact pass-through) passed in CI

Confirmed by Bryn's own Logic test (docs/testing-milestone-0.md, checks 1-6):
- M4 mac mini, macOS Tahoe, Logic 11.2: all passed
- Intel Mac, macOS 12.7.8, Logic 10.7.8: all passed

Not tested: VST3 in any host (Logic loads AU only).

Choices made (see docs/open-questions.md): JUCE 8.0.6, AU type effect (aufx), codes Ptdt/Smru,
runner macos-15 (macos-14 retires 2 Nov 2026).

## Milestone 1: Source audit. DONE (approved 2026-10-25)

Bryn approved the keep/stub/drop list (architecture.md section 6) and decided to follow the source
for every open item M1-a to M1-t. M1-m (output labelling): source mapping for now, Bryn chooses
left/right later. Still undecided and not needed yet: Q1 to Q5 (licensing), v5 feature scope,
Freq as MIDI or parameter.

## Milestone 2: Headless DSP core. DESIGN APPROVED 2026-10-25, coding not started

Written (docs only, nothing built or run):
- architecture.md section 11 (tick scheduler), 12 (sample-rate handling), 13 (scope and numeric
  rules), 14 (verification plan). Section 10 amended (scheduler to src/core, resampler to src/host).
- open-questions.md: M1-t corrected; decisions M2-a to M2-g, each with a proposed default.

Bryn approved the proposed default for M2-a to M2-g. Next: write the code (architecture.md sections 11 to 14), build and test in CI.

Note: CI logs cannot be downloaded in Claude's session (HTTP 403). The workflow writes results as
annotations (archs, minos, auval) so they can be read through the API. The Milestone 2 tests will
do the same.
