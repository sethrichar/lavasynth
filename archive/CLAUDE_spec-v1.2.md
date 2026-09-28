# CLAUDE.md — Voice-Rotating Poly Synth (working title: "Rotor")

> Working title only. Do NOT use "Katla", "Genki", or the Icelandic parameter names in code identifiers,
> UI, bundle IDs, presets, or marketing. The Icelandic names appear below ONLY as a reference key to the manual.
> Spec revision: 1.2 (working). Source: Katla User Manual (`docs/katla.pdf`, local reference only — gitignored).
> Previous spec: `archive/CLAUDE_spec-v1.1.md`. Items the manual does not answer are marked **[OPEN]** and tracked in `OPEN_QUESTIONS.md`.

## What this project is
A VST3 + AU instrument plugin, built in JUCE (C++), closely recreating the behavior of a five-voice,
voice-rotating hybrid polysynth: digital oscillators into an analog-style filter, wavefolder, amp, reverb, and CMOS drive.

Priorities, in order:
1. Voice rotation / allocation engine — faithful
2. Modulation (envelopes, LFOs, control-signal wavefolding, LFO min-maxing) — faithful
3. Wildcard parameters (the 8 randomizing/instability controls) — faithful
4. Filter, wavefolder, drive, reverb — good-sounding analog models, not circuit-exact

## Environment
- macOS. Projects live in `/Users/sethrichardson/Projects/`. This repo: `/Users/sethrichardson/Projects/rotor/`
- JUCE via CMake (not Projucer). JUCE is already at `/Users/sethrichardson/Projects/JUCE` — use it via
  `add_subdirectory(../JUCE JUCE)` (or `-DJUCE_DIR`); don't download another copy. Check its version first.
- Formats: VST3, AU, plus Standalone for quick testing.
- Validate every build with `pluginval` (strictness 5+) and `auval -v aumu <code> <mfr>` for AU.
- `docs/katla.pdf` is the reference manual. Add `docs/*.pdf` to `.gitignore` — never commit it.

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
Note: reverb comes BEFORE drive. The per-voice level slider sits after the amp, but a voice's level
and octave also influence its sub-osc, filter, LFO, and key tracking even when the main osc level is 0 **[OPEN: exact coupling]**.

The "four distortion destinations" are, as best we can tell: OSC-level overdrive, filter resonance overdrive,
wavefolder, CMOS drive **[OPEN: confirm]**.

## Voices (exactly 5, fixed identities — not a pool)
Per voice: **on/off**, **level**, **octave** **[OPEN: range]**, **waveform** (discrete select, ordered by harmonic content):
square, sawtooth, shark-tooth, triangle, sine. Band-limit everything (polyBLEP/minBLEP or wavetables).
- Shark-tooth: asymmetric saw/triangle hybrid (the manual's default "init" tone) **[OPEN: exact shape]**.
- Phase distortion (CZ-style phase-angle warping) is applied to the oscillator; bipolar — negative inverts direction.

## Global section
- **OSC Level** — level of all 5 main oscs; above midpoint gently overdrives (soft saturation).
- **Sub** — level, octave **[OPEN: options, assume −1/−2]**, waveform sine|square. One sub per voice, follows that voice's level + octave.
- **Noise/EXT** — pink noise level; when EXT on, controls external input level instead.
  Noise/EXT passes through a **tilt EQ** (pivot 800 Hz) set by a **Color** slider.
- **Glide** — time scales with interval size (2-octave leap takes noticeably longer than a half step);
  descending glides take ~10% longer than ascending.

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
- Shapes: sine, sawtooth, reverse sawtooth, square, **Volcano** (slewed sample-and-hold, two new random values per cycle).
- Option **LFO Retrigger**: off = free-running, on = reset on every key press.
- Host sync option (LFO CLK): rate snaps to note values.
- Bipolar depth sliders for 8 destinations:
  - Per voice: **phase distortion, filter cutoff, spreader, wavefolder**. LFO on the spreader moves each voice independently.
  - Global stereo (**LFO min-maxing**): **reverb mix, drive mix, effects amount, effects color**. The five LFOs are combined:
    slider up → use the maximum of the five LFO outputs; slider down → use the minimum. This produces complex,
    non-repeating movement because the five rates differ (key tracking).

## Wildcard parameters (8 global sliders, each acting independently per voice)
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
7. **Stereo spreader** [Rökkur] — static pan: voices 2 and 4 left, 3 and 5 right, 1 centered. Bipolar: negative reverses the pan order.
   Mod-envelope modulation also moves voice 1; LFO modulation moves each voice independently. The slider acts as the offset for modulation.
8. **Wavefolder** [Rústir] — per-voice wavefolder amount (West Coast style). Modulated by mod env and LFO; control signal folds at limits.

**[OPEN]** for all 8: depth ranges, rates, burst timing/probability. Start from the manual's descriptions, tune by ear, and keep every constant in one `WildcardTuning.h`.

## Effects (stereo, reverb → drive)
- **Reverb** (inspired by 1980s hardware; e.g. a modulated plate/hall in the Lexicon/EMT-digital vein):
  **Amount** is a macro for tail length + modulation depth + stereo width (low = short, still, narrow; high = long, modulated, wide).
  **Mix** = dry/wet.
- **CMOS drive**: models CMOS-inverter clipping (asymmetric, sharp, dynamic). **Amount** = input gain, **Mix** = dry/wet.
- **Effects Color**: reverb → tank filter + tilt EQ; drive → mid-scoop filter whose dip frequency moves (sweeping it sounds phaser-like).
- Oversample the drive 4×.

## Aftertouch (channel AT now; per-note with MPE later)
Three bipolar-ish controls:
- **Wildcard AT**: up = aftertouch blends in the four pitch wildcards (note detune, wow, flutter, reel drag);
  down = "harmonic clusters": modulates the pitch of voices 1, 2, 4, 5 while voice 3 stays put **[OPEN: intervals]**.
- **Filter cutoff AT**: up raises cutoff, down lowers it.
- **LFO rate AT**: up speeds up, down slows down.

## Global options (from the hardware's DIP switches — plugin settings)
- MIDI/MPE mode (MPE: per-note pitch bend, timbre, aftertouch)
- Round-Robin Reset, Unison Grace Period, Mono Note Priority, LFO Retrigger (see above)
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
- **Row presets**: save/load one section only (e.g. Wildcards + LFO) without touching Voices. 8 slots per row in the hardware;
  in the plugin, a per-section preset menu.

## Beyond the hardware (optional, post-v2 — not in the original)
- "Re-roll voices" button that randomizes per-voice level/octave/waveform within limits.
- Seeded randomness so a bounce can be reproduced exactly.

## Milestones (tag each; one at a time)
- **v1.1** — CMake JUCE skeleton builds VST3/AU/Standalone. One voice: 5 band-limited waveforms → simple LP filter → amp ADSR → out. Passes pluginval.
- **v1.2** — 5 fixed voices with on/off, level, octave, waveform. Round-Robin Forward/Backward/Random + RR Reset. Glide. Allocator unit tests.
- **v1.3** — Unison Staccato/Legato/Mono + Grace Period + Mono Priority. Tests for all 6 modes.
- **v1.4** — Global section: OSC-level overdrive, sub osc, pink noise + tilt EQ. Real filter (LP 24 + BP blend, resonance drive, key tracking). Wavefolder.
- **v1.5** — Amp env loop + key tracking; mod env (5 destinations) with control-signal folding; phase distortion; LFOs (5 shapes incl. Volcano, ranges, key tracking, retrigger); host sync.
- **v1.6** — The 8 wildcard parameters + stereo voice panning. `WildcardTuning.h`.
- **v1.7** — Reverb → CMOS drive, Effects Color, LFO min-maxing to the 4 global FX destinations.
- **v1.8** — Aftertouch (3 modes), MPE, Global Detune / Pitch Drift, sidechain external inputs.
- **v1.9** — Full + row presets, UI pass.
- **v2** — First exported build for testing by others.

## Working rules for Claude Code
- One milestone at a time. Don't start the next until the current one builds, passes pluginval, and is tagged.
- No allocations or locks on the audio thread. Smooth all parameter changes.
- Keep DSP in plain C++ classes separate from JUCE plugin glue so it can be unit-tested.
- When behavior depends on an **[OPEN]** item, implement the simplest reasonable version, leave a `// OPEN:` comment,
  and add or update the entry in `OPEN_QUESTIONS.md`.
- Consult `docs/katla.pdf` when a behavior question comes up; quote the page number in the commit message.
