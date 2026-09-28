# Changelog

Working revisions use decimals (v1.1, v1.2…). Only builds shared with others get whole numbers (v2, v3…).
Each entry: what changed, what's broken, how to roll back.

## Spec history (before code)
- 2026-09-28 — Spec rev 1.1: first draft from public info. (`archive/CLAUDE_spec-v1.1.md`)
- 2026-09-28 — Spec rev 1.2: rewritten from the user manual. (`archive/CLAUDE_spec-v1.2.md`)
- 2026-09-28 — Spec rev 1.3: control details from the cheat sheet. (`archive/CLAUDE_spec-v1.3.md`)
- 2026-09-28 — Spec rev 1.4: cloud (Linux) + GitHub Actions (macOS) build workflow. (`archive/CLAUDE_spec-v1.4.md`)
- 2026-09-28 — Spec rev 1.5: repo is public; spec made self-contained, manufacturer docs kept off-repo.

## Code

### v1.1 — 2026-09-28 — JUCE skeleton, one voice
**Changed**
- CMake project; JUCE pulled with FetchContent, pinned to **JUCE 8.0.15**. Catch2 **v3.7.1** for unit tests.
- Plugin "Rotor" (VST3 + Standalone on Linux; VST3 + AU + Standalone on macOS). Codes: manufacturer `Lvsy`, plugin `Rtr1` (`auval -v aumu Rtr1 Lvsy`).
- One voice, last-note priority: 5 band-limited waveforms (square and saw via polyBLEP; shark-tooth and triangle via polyBLAMP; sine),
  octave −2..+2 → 12 dB TPT state-variable lowpass (cutoff, resonance) → ADSR amp envelope (linear attack, exponential decay/release,
  release up to 1 hour) → level. Cutoff/resonance/level are smoothed. Generic JUCE editor for now.
- DSP lives in `src/dsp/` as plain C++ (no JUCE); 15 unit tests (aliasing, range/DC, pitch, filter response/stability, envelope stages, voice).
- `.github/workflows/macos.yml`: universal build, unit tests, pluginval (strictness 5), auval, uploads a zip of the plugins.
  Also runs on `claude/**` and `milestone/**` branches so milestones can be checked before merging.

**Validated**: unit tests pass (Linux + macOS), Linux pluginval passes at strictness 5 and 10, macOS workflow green (pluginval + auval).

**Broken / not yet done**: only one voice (no allocation modes yet, v1.2). Filter is a placeholder 12 dB SVF (real filter in v1.4).
No custom UI. Shark-tooth shape, envelope minimum times, and velocity response are guesses (see `OPEN_QUESTIONS.md`).

**Roll back**: `git checkout v1.1` (or before this: the spec-only commit `0e30c6b`).
