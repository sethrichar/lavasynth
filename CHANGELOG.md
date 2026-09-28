# Changelog

Working revisions use decimals (v1.1, v1.2…). Only builds shared with others get whole numbers (v2, v3…).
Each entry: what changed, what's broken, how to roll back.

## Spec history (before code)
- 2026-09-28 — Spec rev 1.1: first draft from public info. (`archive/CLAUDE_spec-v1.1.md`)
- 2026-09-28 — Spec rev 1.2: rewritten from the user manual. (`archive/CLAUDE_spec-v1.2.md`)
- 2026-09-28 — Spec rev 1.3: control details from the cheat sheet. (`archive/CLAUDE_spec-v1.3.md`)
- 2026-09-28 — Spec rev 1.4: cloud (Linux) + GitHub Actions (macOS) build workflow. (`archive/CLAUDE_spec-v1.4.md`)
- 2026-09-28 — Spec rev 1.5: repo is public; spec made self-contained, manufacturer docs kept off-repo. (`archive/CLAUDE_spec-v1.5.md`)
- 2026-09-28 — Spec rev 1.6: glide source confirmed (each voice glides from its own previous note).

## Code

### v1.4 — 2026-09-28 — Global section, real filter, wavefolder
**Changed**
- Voices now run **4× oversampled** (two halfband FIR stages back down: flat to 18 kHz, aliasing ≥ 60 dB down).
- **OSC Level**: 0–50% is a clean level for the main oscillators; above 50% the mix is driven into a soft saturator.
- **Sub oscillator** per voice: level, −1/−2 octave, sine/square; follows the voice's octave and glide.
- **Pink noise** (shared source) through a **tilt EQ** pivoting at 800 Hz, bipolar **Noise Color** (±6 dB per side).
- **Filter** replaced with a 4-pole ladder (TPT, tanh in the feedback):
  - Lowpass 24 dB/oct; resonance drives the saturator; self-oscillates within ~1 cent of the cutoff.
  - Bandpass: ladder bandpass tap + 15% of the 12 dB lowpass, hotter resonance.
  - **Filter Key Track** 0–100% (100% = cutoff follows pitch 1:1, reference middle C).
- **Wavefolder** (sine folder, West-Coast style) after the filter, 0 = exact bypass.
- Fast tanh (rational approximation, ≤ 1e-4 error) in the voice path — voice CPU down ~35%.
- `devtools/RenderCheck.cpp` (`-DROTOR_BUILD_DEVTOOLS=ON`): offline render of the full plugin, prints CPU/peak/RMS, can write a WAV.
- v1.1's 12 dB `LowpassFilter` is deprecated but kept (with its tests), per the no-delete rule.
- New parameters: `oscLevel`, `subLevel`, `subOctave`, `subWaveform`, `noiseLevel`, `noiseColor`, `filterMode`, `keyTrack`, `fold`.
  `resonance` now drives the ladder (same ID, stronger effect).
- 74 unit tests (19 new: ladder response/self-oscillation/stability, key tracking, pink noise spectrum, tilt EQ, overdrive,
  wavefolder, decimator passband/stopband, sub octave, noise path).

**Validated**: unit tests pass, Linux pluginval passes at strictness 5 and 10, macOS workflow green (pluginval + auval).
Full-plugin render: 5 voices on a heavy patch ≈ 11% of one 2.1 GHz cloud core, peak −2.3 dBFS, no NaNs.

**Broken / not yet done**: the bandpass is louder than the lowpass at high resonance (self-oscillation ~16 dB hotter) — tune by ear.
Wavefolder is a single global knob until it becomes a mod destination (v1.5) and a wildcard slider (v1.6).
New guesses logged in `OPEN_QUESTIONS.md`.

**Roll back**: `git checkout v1.3`.

### v1.3 — 2026-09-28 — Unison modes: Staccato, Legato, Mono
**Changed**
- Voice Mode gains **Staccato**, **Legato**, **Mono** (appended after Forward/Backward/Random, so saved v1.2 settings keep their mode).
- Unison: all enabled voices spread over the held notes, re-spread whenever the key count changes; at most 5 notes (newest win).
  - Staccato: every voice retriggers on every key press.
  - Legato: voices only change pitch (gliding if Glide is on) while any key is held; retrigger after all keys are released.
  - Mono: all voices on one note; **Mono Note Priority** Last (default) / Lowest / Highest. Retriggers only when the sounding note changes.
- **Unison Grace Period** option: when you lift keys of a chord, those voices fade out on their own notes; the rest wait 80 ms before
  re-spreading. Without it, the last key held grabs all voices (monophonic release).
- Allocator now returns an action per voice (trigger / move / release) so one key can affect several voices.
- 55 unit tests (18 new unison/mono/grace tests).

**Validated**: unit tests pass, Linux pluginval passes at strictness 5 and 10, macOS workflow green (pluginval + auval).

**Broken / not yet done**: nothing known. New guesses logged in `OPEN_QUESTIONS.md` (voice split, Mono retrigger, grace length,
whether releasing a key in Staccato retriggers).

**Roll back**: `git checkout v1.2`.

### v1.2 — 2026-09-28 — 5 voices, Round-Robin allocation, glide
**Changed**
- Five fixed voices, each with On/Off, Level, Octave (−2..+2) and Waveform. Filter, amp envelope and glide are shared controls.
- Voice allocator (`src/dsp/VoiceAllocator.h`, pure logic): Round-Robin **Forward**, **Backward**, **Random**; disabled voices skipped;
  rotation steals the next voice even while it sounds; **Round-Robin Reset** option (back to voice 1 when all keys are up).
  Random is seeded (reproducible) and never repeats the same voice twice in a row.
- Glide: constant rate in semitones, so time scales with the interval; descending takes 10% longer. Knob = seconds per octave (0 = off).
- Parameters: new `voiceMode`, `roundRobinReset`, `glide`, `voice1On`…`voice5Waveform`. Retired v1.1's single-voice `waveform` and `octave`.
  `level` is now labelled Master Level (same ID).
- 37 unit tests (22 new: every allocator mode/option, enabling/disabling voices mid-note, glide timing).

**Validated**: unit tests pass, Linux pluginval passes at strictness 5 and 10, macOS workflow green (pluginval + auval).

**Broken / not yet done**: unison modes (Staccato/Legato/Mono) come in v1.3. Voices are summed to mono-centre (spreader is v1.6).
New guesses logged in `OPEN_QUESTIONS.md`: Backward + Reset start voice, glide source note and curve, disabling a sounding voice.

**Roll back**: `git checkout v1.1`.

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
