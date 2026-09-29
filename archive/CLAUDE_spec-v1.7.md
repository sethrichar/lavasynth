# CLAUDE.md — Voice-Rotating Poly Synth (working title: "Rotor")

> Working title only. Do NOT use "Katla", "Genki", or the Icelandic parameter names in code identifiers,
> UI, bundle IDs, presets, or marketing. The Icelandic names appear below ONLY as a reference key to the manual.
> Spec revision: 1.7 (working). Sources: the Katla User Manual and controls cheat sheet (kept on the owner's Mac only —
> NOT in this public repo). This spec is the complete reference; everything needed from those documents is summarized here.
> Previous specs: `archive/`. Items the manual does not answer are marked **[OPEN]** and tracked in `OPEN_QUESTIONS.md`.

## What this project is
A VST3 + AU instrument plugin, built in JUCE (C++), closely recreating the behavior of a five-voice,
voice-rotating hybrid polysynth: digital oscillators into an analog-style filter, wavefolder, amp, reverb, and CMOS drive.

Priorities, in order:
1. Voice rotation / allocation engine — faithful
2. Modulation (envelopes, LFOs, control-signal wavefolding, LFO min-maxing) — faithful
3. Wildcard parameters (the 8 randomizing/instability controls) — faithful
4. Filter, wavefolder, drive, reverb — good-sounding analog models, not circuit-exact

## Environment (two machines)
**Cloud (Claude Code sessions, Linux, this GitHub repo):** writes all code, builds VST3 + Standalone for Linux,
runs unit tests and Linux `pluginval`. Cannot build AU or run `auval` (macOS only) and cannot open the owner's DAW.
- Get JUCE with CMake `FetchContent`, pinned to a release tag (latest JUCE 8.x at setup; record the tag in CHANGELOG).
  Do not depend on any local JUCE path.
- Linux build deps (install if missing): `build-essential cmake pkg-config libasound2-dev libfreetype-dev libfontconfig1-dev
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libgl1-mesa-dev libcurl4-openssl-dev libwebkit2gtk-4.1-dev`.
  Disable JUCE web browser and CURL in CMake (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`) to keep deps small.
- Linux pluginval: download the Linux release zip from github.com/Tracktion/pluginval/releases into `tools/` (gitignored),
  run at strictness 5+ against the VST3.
- Build dir `build/` is gitignored. Work on a branch per milestone (`milestone/v1.1`), merge to `main` when it passes, then tag.

**macOS (GitHub Actions + owner's Mac):**
- `.github/workflows/macos.yml` on every push to `main` and every tag: macOS runner builds VST3 + AU (universal: arm64 + x86_64),
  runs `pluginval` (strictness 5) and `auval -v aumu <code> <mfr>`, uploads the built plugins as an artifact.
  Create this workflow in milestone v1.1.
- Owner's Mac: pluginval at `/Applications/pluginval.app/Contents/MacOS/pluginval`; projects in `/Users/sethrichardson/Projects/`.
  The owner pulls the repo (or downloads the Actions artifact) to listen and test in his DAW.
- Formats: VST3, AU, plus Standalone for quick testing.
- This repo is public. Never commit the manufacturer's manual, cheat sheet, images, or audio. `docs/*.pdf` is gitignored.

## Version control rules (important to the owner)
- Commit early and often. Never delete files, branches, or history — deprecate, move to `/archive`, or revise up.
- Working revisions increment by decimal: `v1.1`, `v1.2`, `v1.3`… Tag each completed milestone (`git tag v1.2`).
- Only exported/shared builds get whole numbers: `v2`, `v3`…
- Keep `CHANGELOG.md` updated at every tag: what changed, what's broken, how to roll back.
- When this spec is revised, copy the old one to `archive/CLAUDE_spec-v<rev>.md` first.

## Signal path (per the manual — order matters)
```
Per voice (×5):
  [Main osc (voice waveform) + Sub osc + Noise/EXT→tilt EQ]  ← Global level sliders
      → OSC-level overdrive (above midpoint)
      → Filter (LP 24dB or BP)            ← resonance overdrive
      → Wavefolder ("Rústir")
      → Amp (amp envelope)
      → Per-voice level slider
      → Stereo pan (spreader, "Rökkur")
Sum (stereo) → Reverb → CMOS Drive → Master level → out
```
Confirmed by the cheat sheet's numbered audio path: Main OSC → Filter → Wavefolder → Amp → Per-voice level → Reverb → Distortion.
Note: reverb comes BEFORE drive. The per-voice level slider sits after the amp, but a voice's level
and octave also influence its sub-osc, filter, LFO, and key tracking even when the main osc level is 0 **[OPEN: exact coupling]**.

The "four distortion destinations" are, as best we can tell: OSC-level overdrive, filter resonance overdrive,
wavefolder, CMOS drive **[OPEN: confirm]**.

## Voices (exactly 5, fixed identities — not a pool)
Per voice (3 sliders + button, left to right): **level** (continuous), **octave** (5 steps: −2, −1, 0, +1, +2),
**waveform** (5-step slider, top to bottom = most to least harmonics): square, sawtooth, shark-tooth, triangle, sine.
Plus an **on/off** button per voice. Band-limit everything (polyBLEP/minBLEP or wavetables).
- Shark-tooth: asymmetric saw/triangle hybrid (the manual's default "init" tone) **[OPEN: exact shape]**.
- Phase distortion (CZ-style phase-angle warping) is applied to the oscillator; bipolar — negative inverts direction.

## Global section
- **OSC Level** — level of all 5 main oscs; above midpoint gently overdrives (soft saturation).
- **Sub** — level, octave −1 | −2 (button toggle), waveform sine | square (button toggle). One sub per voice, follows that voice's level + octave.
- **Noise/EXT** — pink noise level; when EXT on, controls external input level instead.
  Noise/EXT passes through a **tilt EQ** (pivot 800 Hz) set by a bipolar **Color** slider (center = flat).
- **Glide** — time scales with interval size (2-octave leap takes noticeably longer than a half step);
  descending glides take ~10% longer than ascending. Each voice glides from its own previous note (not the last key played).

## Voice allocation (6 modes)
Round-Robin (each note assigned to the next voice in sequence):
1. **Forward** — 1→5, wraps
2. **Backward** — 5→1, wraps
3. **Random** — random voice **[OPEN: repeats allowed? assume no immediate repeat]**
- Disabled voices are skipped.
- Rotation steals: the next voice in sequence takes the note even if still sounding **[OPEN: confirm]**.
- Option **Round-Robin Reset**: off = continue from last-used voice; on = return to voice 1 whenever all keys are released.

Unison (all active voices assigned to held notes, redistributed as key count changes; max 5 voices in use):
4. **Staccato** — envelopes retrigger on every note trigger
5. **Legato** — envelopes retrigger only when all keys have been released
6. **Mono** — no polyphony; all voices on one note. Option **Mono Note Priority**: Last (default) | Lowest | Highest.
- Option **Unison Grace Period**: when releasing a chord, the last-lifted note would normally grab all voices
  (monophonic release). With grace on, add a short release-timing offset so voices fade out on their own notes.
- **[OPEN]** how voices split across 2–4 held notes (e.g. 3+2 for two notes?) and which voices go where.

Unit-test the allocator as pure logic, independent of audio. Every mode, every option, enable/disable voices mid-note.

## Amp envelope (5 envelopes, one shared ADSR control set)
- Key-tracking knob: clockwise = higher notes faster, counter-clockwise = lower notes faster.
- **Loop**: cycles after attack+decay. Raising sustain shortens attack and decay (faster loop) and changes depth.
  Releasing before decay ends → go to release.
- Fastest loop (A, D, S all minimum) = C1, 32.7 Hz → audio-rate AM, tunable per note via key tracking.
- Max release ≈ 1 hour. Use exponential slider-to-time mapping **[OPEN: min times]**.
- Host sync option (ENV CLK): envelope sliders snap to note values (straight, triplet, dotted).

## Mod envelope (5 envelopes, one shared control set)
- Same shape/loop/key-tracking behavior as the amp envelope.
- Bipolar depth sliders for 5 destinations: **phase distortion, filter cutoff, LFO rate, spreader (Rökkur), wavefolder (Rústir)**.
- **Control-signal wavefolding**: for phase distortion, spreader, and wavefolder destinations, when base + modulation
  exceeds the parameter's range, it folds back (reflects) instead of clipping. Implement as a reusable `foldIntoRange()`.

## Filter (5, one per voice)
- **Lowpass**: 4-pole, 24 dB/oct, character between MS-20 and SEM. Resonance adds overdrive/distortion at high settings.
- **Bandpass**: blend of 36 dB/oct bandpass + a small amount of 12 dB/oct lowpass. Reacts more aggressively to resonance.
- Key-tracking knob. At full key tracking + max resonance, filters self-oscillate in tune (playable as oscillators).
- Hardware filter auto-tuning is not needed (digital filters track exactly). Optional later: a small per-voice
  cutoff mistuning to mimic analog spread.
- Model: ZDF/TPT ladder or cascade with nonlinearities (tanh in feedback). Oversample the nonlinear stages.

## LFO (5, one per voice, shared controls)
- One rate knob + key-tracking offset. Range switch: **Slow 0.064–4.8 Hz**, **Fast 1.02–65.4 Hz** (10 octaves total).
  With key tracking centered, rate is tuned to C at both extremes. Fast + max rate + full key tracking = audio-rate FM/AM tracking pitch.
- Shape slider (5 steps, top to bottom): **Volcano**, square, reverse saw, saw, sine. **Volcano** (slewed sample-and-hold, two new random values per cycle).
- Option **LFO Retrigger**: off = free-running, on = reset on every key press.
- Host sync option (LFO CLK): rate snaps to note values.
- Rate slider (unipolar) + range button (slow/fast) + key-tracking knob.
- Bipolar depth sliders (center = off) for 8 destinations:
  - Per voice: **phase distortion, filter cutoff, spreader, wavefolder**. LFO on the spreader moves each voice independently.
  - Global stereo (**LFO min-maxing**): **reverb mix, drive mix, effects amount, effects color**. The five LFOs are combined:
    slider up → use the maximum of the five LFO outputs; slider down → use the minimum. This produces complex,
    non-repeating movement because the five rates differ (key tracking).

## Wildcard parameters (8 global UNIPOLAR sliders, 0 = off, each acting independently per voice)
Reference key: manual names in brackets, for lookup only; use our own names in code and UI.
Pitch group (all four can also be blended in with aftertouch; see below):
1. **Note detune** [Móða] — new random detune per voice on every key press (vintage out-of-tune drift).
2. **Wow** [Kvika] — slow, gradual pitch fluctuation per voice, slewed random.
3. **Flutter** [Skjálfti] — faster pitch fluctuation per voice (tape flutter).
4. **Reel drag** [Glóð] — brief, random bursts of detuning (like pressing a finger on a tape reel).

Other:
5. **Chaos** [Aska] — random modulation of per-voice volume and filter cutoff, plus short bursts of noise.
6. **Envelope scatter** [Skriða] — random attack/decay/release times on the amp envelope, per note trigger. Rules:
   - attack = 0 and decay > 0 → randomize decay only
   - attack > 0 and decay = 0 → randomize attack only
   - both 0 → randomize both
   - (both > 0 → **[OPEN]**, assume randomize both)
   - always randomize release unless release is at maximum
7. **Stereo spreader** [Rökkur] — static pan: voices 2 and 4 left, 3 and 5 right, 1 centered; the slider sets the width.
   Negative mod-envelope depth reverses the pan order; mod-envelope modulation also moves voice 1; LFO modulation moves
   each voice independently. The slider acts as the offset for modulation.
8. **Wavefolder** [Rústir] — per-voice wavefolder amount (West Coast style). Modulated by mod env and LFO; control signal folds at limits.

**[OPEN]** for all 8: depth ranges, rates, burst timing/probability. Start from the manual's descriptions, tune by ear, and keep every constant in one `WildcardTuning.h`.

## Effects (stereo, reverb → drive)
- **Reverb** (inspired by 1980s hardware; e.g. a modulated plate/hall in the Lexicon/EMT-digital vein):
  **Amount** is a macro for tail length + modulation depth + stereo width (low = short, still, narrow; high = long, modulated, wide).
  **Mix** = dry/wet.
- **CMOS drive**: models CMOS-inverter clipping (asymmetric, sharp, dynamic). **Amount** = input gain, **Mix** = dry/wet.
- Reverb Amount, Reverb Mix, Drive Amount, Drive Mix are unipolar. **Effects Color** is bipolar (center = neutral): reverb → tank filter + tilt EQ; drive → mid-scoop filter whose dip frequency moves (sweeping it sounds phaser-like).
- Oversample the drive 4×.

## Aftertouch (channel AT now; per-note with MPE later)
Three bipolar sliders (center = off):
- **Wildcard AT**: up = aftertouch blends in the four pitch wildcards (note detune, wow, flutter, reel drag);
  down = "harmonic clusters": modulates the pitch of voices 1, 2, 4, 5 while voice 3 stays put **[OPEN: intervals]**.
- **Filter cutoff AT**: up raises cutoff, down lowers it.
- **LFO rate AT**: up speeds up, down slows down.

## Global options (from the hardware's DIP switches — plugin settings)
- MIDI/MPE mode (MPE: per-note pitch bend, timbre, aftertouch)
- Round-Robin Reset, Unison Grace Period, Mono Note Priority, LFO Retrigger (see above)
- **Mod Wheel** (from cheat sheet; not in manual): "Katla" = wheel blends in the wildcard params | "Pitch LFO" = wheel adds
  LFO → pitch (vibrato) **[OPEN: which wildcards, depths]**. Note the cheat sheet lists Mod Wheel where the manual lists
  Mono Note Priority — firmware likely changed; we keep both as plugin settings.
- **Global Detune** (±1 semitone, uniform) OR **Pitch Drift** mode: each voice has its own center note; the further a played
  note is from that voice's center, the more it drifts out of tune (per-voice tracking error). Turning the other way detunes globally.
- Hardware-only items to skip: slider latch/catch, MIDI channel DIP, power, recovery mode, external input level standard, per-voice MIDI out.

## External inputs (later milestone)
Plugin sidechain input replaces the five hardware inputs. Routing rules to emulate:
- Mono (input 1 only) → all voices.
- Stereo pair (inputs 1+2) → hard-panned, and also distributed to voices 3, 4, 5.
- EXT replaces noise and goes through the same tilt EQ, then the full voice path.

## Presets
- Full presets (all parameters) via standard host preset handling + our own preset browser.
- **Row presets**: the panel has 3 rows, each with its own 8 preset slots; a FULL/ROW switch picks whole-panel vs per-row recall.
  - Row 1: Voices (mode + 5 voices), Global (OSC, Sub, Noise/EXT, Glide)
  - Row 2: Aftertouch, Amp Envelope, Mod Envelope, Filter
  - Row 3: LFO, Effects, Wildcards
  In the plugin: a preset menu per row plus full presets. Master-section toggles (EXT, sync) are probably not stored per row **[OPEN]**.

## Beyond the hardware — voicing selectors (in the plugin since v1.6, owner decision 2026-09-29)
- **Envelope Curve** (applies to amp + mod envelopes): Rotor, Punchy (Minimoog-inspired), Vintage Poly (Prophet-5/CEM3310-
  inspired), Snappy Digital (Juno-106-inspired), Linear. Default Rotor.
- **Filter Character**: Rotor (ladder), Transistor Ladder (Moog-inspired), OTA Cascade (CEM3320-inspired), State Variable
  (SEM-inspired), Screaming 12dB (MS-20-inspired). Default Rotor.
- Both stay as permanent controls, each with its own button in the final UI (v1.9). UI names stay generic (no brand names).

## Beyond the hardware (optional, post-v2 — not in the original)
- "Re-roll voices" button that randomizes per-voice level/octave/waveform within limits.
- Seeded randomness so a bounce can be reproduced exactly.

## Milestones (tag each; one at a time)
- **v1.1** — CMake JUCE skeleton (FetchContent) builds VST3/Standalone on Linux; macOS Actions workflow builds VST3/AU and runs auval. One voice: 5 band-limited waveforms → simple LP filter → amp ADSR → out. Passes pluginval.
- **v1.2** — 5 fixed voices with on/off, level, octave, waveform. Round-Robin Forward/Backward/Random + RR Reset. Glide. Allocator unit tests.
- **v1.3** — Unison Staccato/Legato/Mono + Grace Period + Mono Priority. Tests for all 6 modes.
- **v1.4** — Global section: OSC-level overdrive, sub osc, pink noise + tilt EQ. Real filter (LP 24 + BP blend, resonance drive, key tracking). Wavefolder.
- **v1.5** — Amp env loop + key tracking; mod env (5 destinations) with control-signal folding; phase distortion; LFOs (5 shapes incl. Volcano, ranges, key tracking, retrigger); host sync.
- **v1.6** — The 8 wildcard parameters + stereo voice panning. `WildcardTuning.h`.
- **v1.7** — Reverb → CMOS drive, Effects Color, LFO min-maxing to the 4 global FX destinations.
- **v1.8** — Aftertouch (3 modes), MPE, Global Detune / Pitch Drift, sidechain external inputs.
- **v1.9** — Full + row presets, UI pass (including buttons for Envelope Curve and Filter Character). Layout can follow the hardware's 3-row functional grouping, but the visual design,
  colors, and names must be our own.
- **v2** — First exported build for testing by others.

## Working rules for Claude Code
- One milestone at a time. Don't start the next until the current one builds, passes pluginval, and is tagged.
- No allocations or locks on the audio thread. Smooth all parameter changes.
- Keep DSP in plain C++ classes separate from JUCE plugin glue so it can be unit-tested.
- When behavior depends on an **[OPEN]** item, implement the simplest reasonable version, leave a `// OPEN:` comment,
  and add or update the entry in `OPEN_QUESTIONS.md`.
- When a behavior question isn't answered here, don't guess silently: pick the simplest version, mark it `// OPEN:`,
  and add it to `OPEN_QUESTIONS.md` for the owner to answer.
