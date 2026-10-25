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

## Milestone 1: Source audit. WRITTEN, waiting for your review

Delivered (no code changes, nothing built or run):
- docs/architecture.md: timing domains, signal path, dependencies of filter.c, params.c,
  rotation.c and the limiter/slew code, hardware-only inventory, keep/stub/drop table,
  licence findings, numerical-fidelity notes.
- docs/open-questions.md: Q6, Q7, Q8 answered; kickoff-pack list settled where the source
  decides it; 20 manual-versus-source items (M1-a to M1-t) for you to decide.

Key findings: `limiter.c` is never called (no limiter in v5.1.1); no `arm_*` function is called, so
no CMSIS code is needed; `rotary.c/.h` is GPLv3 and hardware-only (stays out); stmlib is not used.

Milestone 1 is done when you approve the plan and the keep/stub/drop list. Not approved yet.

## Next: Milestone 2, headless DSP core (after you approve Milestone 1)

Note: CI logs cannot be downloaded in Claude's session (HTTP 403). The workflow writes results as
annotations (archs, minos, auval) so they can be read through the API.
