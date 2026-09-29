# Changelog

Working revisions use decimals (v1.1, v1.2…). Only builds shared with others get whole numbers (v2, v3…).
Each entry: what changed, what's broken, how to roll back.

## Spec history (before code)
- 2026-09-28 — Spec rev 1.1: first draft from public info. (`archive/CLAUDE_spec-v1.1.md`)
- 2026-09-28 — Spec rev 1.2: rewritten from the user manual. (`archive/CLAUDE_spec-v1.2.md`)
- 2026-09-28 — Spec rev 1.3: control details from the cheat sheet. (`archive/CLAUDE_spec-v1.3.md`)
- 2026-09-28 — Spec rev 1.4: cloud (Linux) + GitHub Actions (macOS) build workflow. (`archive/CLAUDE_spec-v1.4.md`)
- 2026-09-28 — Spec rev 1.5: repo is public; spec made self-contained, manufacturer docs kept off-repo. (`archive/CLAUDE_spec-v1.5.md`)
- 2026-09-28 — Spec rev 1.6: glide source confirmed (each voice glides from its own previous note). (`archive/CLAUDE_spec-v1.6.md`)
- 2026-09-29 — Spec rev 1.7: Envelope Curve and Filter Character become permanent selectors (buttons in the final UI).

## Code

### v1.8 — 2026-09-29 — Aftertouch, MPE, Global Detune / Pitch Drift, mod wheel, EXT input
**Changed**
- **Aftertouch** (channel pressure; per-note pressure in MPE; poly AT treated as pressure), three bipolar sliders:
  - **AT > Wildcards / Clusters**: up = pressure blends in the four pitch wildcards; down = "harmonic clusters" —
    voices 1, 2, 4, 5 move to −12, −5, +7, +12 semitones at full pressure while voice 3 stays put.
  - **AT > Cutoff** (±3 octaves) and **AT > LFO Rate** (±3 octaves).
- **MPE** (lower zone): channel 1 = manager (±2 st bend, pressure to all), channels 2–16 = per-note bend (±48 st),
  pressure and timbre (CC74 → cutoff ±2 oct). In unison, every voice on a note follows that note's expression.
  Normal MIDI: pitch bend ±2 st, channel pressure and CC74 apply to all voices (latest message wins).
- **Mod Wheel** option: Wildcards (wheel blends in the pitch wildcards) or Pitch LFO (vibrato from each voice's LFO, ±1 st).
- **Tune**: Global Detune (±1 semitone, all voices) or Pitch Drift (each voice has its own centre note and tracking error;
  up to ±12 cents per octave away from the centre; the negative side still detunes globally, per the manual).
- **EXT input**: a sidechain input bus (off by default). With EXT on it replaces the noise, goes through the same
  Noise Color tilt EQ and the whole voice path (so it only sounds while notes are held). Mono input → all voices;
  stereo → voice 1 left, voice 2 right, voices 3–5 both.
- All new constants in `src/dsp/PerformanceTuning.h`; expression tracking in `src/dsp/Expression.h` (pure logic).
- `RotorRender`: `--ext` (sidechain test), `--mpe`, and `--pitch-check` (verifies a per-note MPE bend lands in tune).
- 138 unit tests (10 new).

**Validated**: unit tests pass, Linux pluginval passes at strictness 5 and 10 (incl. sidechain layouts), macOS workflow
green (pluginval + auval). Full-plugin checks: EXT audible only while notes are held; MPE +12 st per-note bend → 879–880 Hz.

**Broken / not yet done**: MPE zone/bend range are fixed (lower zone, ±48) — no MPE configuration messages yet.
The EXT input's level is the Noise Level slider (as on the hardware). Ranges are guesses — see OPEN_QUESTIONS.

**Roll back**: `git checkout v1.7`.

### v1.7 — 2026-09-29 — Effects: reverb → CMOS drive, Effects Color, LFO min-maxing
**Changed**
- **Reverb**: stereo plate after Dattorro's classic design (the 1980s digital-plate topology), modulated tank.
  **Amount** is a macro — low = short, still, narrow; high = long (~10 s), modulated, wide. **Mix** = dry/wet
  (equal-power, so the level holds through the middle).
- **CMOS drive** (after the reverb, per the manual): asymmetric, sharp clipping with a level-dependent bias (the
  "dynamic" part — the harmonic mix breathes), 4× oversampled; dry/wet mixed inside the oversampled domain so they stay
  aligned. **Amount** = input gain (up to +36 dB) with loudness makeup (level stays within ~1 dB); **Mix** = dry/wet.
- **Effects Color** (bipolar, centre = neutral): reverb → tank damping + tilt EQ on the tail; drive → a mid-scoop that
  moves from ~180 Hz to ~5.7 kHz and deepens away from centre (sweeping it sounds phaser-like).
- **LFO min-maxing** to Reverb Mix, Drive Mix, Effects Amount (moves both amounts), Effects Color: slider up = the
  maximum of the five voice LFOs, down = the minimum.
- Spec rev 1.7: Envelope Curve and Filter Character are permanent selectors (buttons in the final UI).
- Fixed `RotorRender` ignoring `--plain` unless it came second.
- 128 unit tests (9 new: reverb bypass/tail/width/colour/stability, drive latency/distortion/asymmetry/bounds/scoop, min-max).

**Validated**: unit tests pass, Linux pluginval passes at strictness 5 and 10, macOS workflow green (pluginval + auval).
Busy-patch render with both effects ≈ 19% of one 2.1 GHz cloud core, no NaNs.

**Broken / not yet done**: the drive adds 0.8 ms of latency (not reported to the host; inaudible for a synth).
Effect ranges are first guesses — see OPEN_QUESTIONS.

**Roll back**: `git checkout v1.6`.

### v1.6 — 2026-09-29 — Wildcards, stereo spreader, voicing lab
**Changed**
- **The 8 wildcards** (unipolar, 0 = off, each voice independent, all constants in `src/dsp/WildcardTuning.h`):
  Note Detune (new random offset per key press), Wow (slow drift), Flutter (fast wobble), Reel Drag (random downward
  pitch dips that recover), Chaos (volume dips, cutoff wander, short noise bursts), Envelope Scatter (random amp A/D/R per
  trigger, following the manual's rules), Stereo Spread, and Wavefolder (the existing `fold`).
- **Stereo**: voices are panned (voice 1 centre, 2 & 4 left, 3 & 5 right) and the plugin now outputs true stereo.
  Mod env → Spread widens/narrows and reverses the order past zero (and moves voice 1); LFO → Spread moves each voice on
  its own; everything folds at the edges. Constant-power panning, level-matched to v1.5 for centred voices.
- **Voicing lab** (owner feedback: envelope shape and filter voicing need work). Two temporary selectors to compare by ear:
  - **Envelope Curve**: Rotor (v1.5), Punchy (Minimoog-inspired), Vintage Poly (Prophet-5/CEM3310-inspired),
    Snappy Digital (Juno-106-inspired), Linear. All keep exact stage times and the 32.7 Hz fastest loop.
  - **Filter Character**: Rotor (v1.4 ladder), Transistor Ladder (Moog-inspired, per-stage saturation, bass thins),
    OTA Cascade (CEM3320/Prophet-5-inspired, cleaner), State Variable (Oberheim SEM-inspired, 12 dB, round),
    Screaming 12dB (MS-20-inspired, hard asymmetric resonance). All self-oscillate in tune; level-matched within ~3.5 dB.
  Once a curve and a character are chosen, they become the fixed voicing and these selectors are retired.
- Bandpass at high resonance now sits ~6 dB above the lowpass (was ~16 dB) — fixes the v1.4 known issue.
- `RotorRender` gains `--plain`, `--seconds=N` and `paramId=value` overrides for A/B renders.
- 119 unit tests (19 new).

**Validated**: unit tests pass, Linux pluginval passes at strictness 5 and 10, macOS workflow green (pluginval + auval).
Busy-patch render ≈ 15% of one 2.1 GHz cloud core, true stereo, no NaNs.

**Broken / not yet done**: aftertouch blending of the pitch wildcards is v1.8. Wildcard depths/rates are first guesses.

**Roll back**: `git checkout v1.5`.

### v1.5 — 2026-09-29 — Modulation: looping envelopes, mod envelope, LFOs, phase distortion, host sync
**Changed**
- **Envelopes** (amp + mod, one each per voice, shared controls): analog-style RC curves; stage times are full-swing times,
  so higher sustain shortens decay. **Loop** mode cycles attack → decay while held; fastest loop (A, D, S at minimum) is
  C1 = 32.7 Hz. **Key Track** (bipolar): + = higher notes faster, − = lower notes faster, ±100% = double/halve per octave.
  Minimum stage time is now 15.3 ms (half the fastest loop) — see OPEN_QUESTIONS.
- **Mod envelope** with bipolar depths to phase distortion, cutoff (±5 oct), LFO rate (±4 oct), spreader (stored; used in
  v1.6) and wavefolder.
- **Control-signal wavefolding** (`foldIntoRange`): phase distortion, wavefolder (and later spreader) reflect at their limits.
- **LFOs** (5, one per voice): Volcano (slewed S&H, two random values per cycle, own seed per voice), square, reverse saw,
  saw, sine; **Slow** 0.064–4.8 Hz / **Fast** 1.02–65.4 Hz (ends tuned to C); key tracking; **Retrigger**; depths to phase
  distortion, cutoff (±4 oct), spreader (stored; used in v1.6), wavefolder. Band-limited so audio-rate LFO works.
- **Phase distortion** (CZ-style single-breakpoint warp) on the main oscillators, bipolar, plus a PD offset knob.
- **Host sync**: Envelope Sync snaps A/D/R to note values (1/64 to 16 bars, straight/triplet/dotted); LFO Sync snaps the
  LFO period. Tempo comes from the host (120 BPM if none).
- Modulation (cutoff, fold, PD, LFO rate) refreshes every 8 samples at the 4× voice rate (24 kHz at 48 kHz).
- 100 unit tests (26 new).

**Validated**: unit tests pass, Linux pluginval passes at strictness 5 and 10, macOS workflow green (pluginval + auval).
Full-plugin render with every modulator active: ≈ 15% of one 2.1 GHz cloud core, peak −1.5 dBFS, no NaNs.

**Broken / not yet done**: spreader depths do nothing until v1.6 (stereo panning). LFO min-maxing to the effects is v1.7.
LFO sync snaps the rate only (the LFO doesn't lock to the bar position). Many ranges are guesses — see OPEN_QUESTIONS.

**Roll back**: `git checkout v1.4`.

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
