# CLAUDE.md — Voice-Rotating Poly Synth (working title: "Rotor")

> Working title only. Do NOT use "Katla" or "Genki" in code, UI, bundle IDs, or marketing.
> Spec revision: v1.1 (working). Manual-dependent items are marked **[CONFIRM]**.

## What this project is
A VST3 + AU instrument plugin, built in JUCE (C++), that closely recreates the behavior of the Genki Katla:
a five-voice, voice-rotating polysynth with digital oscillators into an analog-modeled filter/amp/drive path.

Priorities, in order:
1. Voice rotation / allocation engine (faithful)
2. Modulation section, especially per-voice randomization (faithful)
3. "Wildcard" instability layer (faithful in feel; exact behavior **[CONFIRM]**)
4. Analog-modeled filter + drive, reverb (good-sounding, not circuit-exact)

## Environment
- macOS. Projects live in `/Users/sethrichardson/Projects/`. This repo: `/Users/sethrichardson/Projects/rotor/`
- JUCE via CMake (not Projucer). Formats: VST3, AU, plus Standalone for quick testing.
- Validate every build with `pluginval` (strictness 5+) and `auval -v aumu <code> <mfr>` for AU.

## Version control rules (important to the owner)
- Commit early and often. Never delete files, branches, or history — deprecate, move to `/archive`, or revise up.
- Working revisions increment by decimal: `v1.1`, `v1.2`, `v1.3`… Tag each completed milestone (`git tag v1.2`).
- Only exported/shared builds get whole numbers: `v2`, `v3`…
- Keep `CHANGELOG.md` updated at every tag: what changed, what's broken, how to roll back.

## Architecture
```
MIDI/MPE in → Allocator (6 modes) → 5 × Voice → sum → Drive (stereo) → Reverb → out
                                         ↑
                     Wildcard layer + per-voice mod (LFO, looping env)
```
Each **Voice** (fixed, exactly 5, always instantiated — voices are identities, not a pool):
- Digital oscillator with waveform morph, level, tuning (per-voice values)
- Dedicated sub-oscillator (global waveform + octave controls)
- Resonant filter, lowpass/highpass modes (analog-modeled, ZDF/TPT with nonlinearity)
- VCA
- Own LFO and own looping envelope
- External input slot (stub only until later milestone)

Per-voice parameters use a **base value + per-voice offset** model so a global control moves all voices
while each voice keeps its own character. UI later: sliders per voice + a global/spread control.

## Allocation modes (6)
Round robin (each note goes to the next voice in sequence):
1. **Forward** — cycles 1→5
2. **Backward** — cycles 5→1
3. **Random** — picks a voice at random **[CONFIRM: avoid repeats? skip held voices?]**

Unison (active voices assigned to held notes, redistributed as key count changes):
4. **Staccato** — envelopes retrigger on every note trigger
5. **Legato** — envelopes retrigger only when all keys have been released
6. **Mono** — polyphony off, all voices on the same note

**[CONFIRM]** voice stealing rules when all 5 are busy in round robin; how unison splits voices across e.g. 2 or 3 held notes; per-voice detune/spread in unison.

## Modulation
- Per voice: 1 LFO + 1 looping envelope (envelope can loop so it behaves like a shaped LFO).
- Per-voice modulation depth.
- **Randomize** is a first-class feature:
  - Per-note random offsets on trigger (amount per destination)
  - A "randomize voices" action that re-rolls per-voice offsets within a user-set range
  - Randomization must be seedable and recallable (store seed + results in the preset) so a patch sounds the same on reload.
- Destinations **[CONFIRM full list]**: pitch, waveform morph, filter cutoff, resonance, level, pan.
- MPE: pressure, pitch bend, timbre (CC74), aftertouch — per note (later milestone).

## Wildcard layer
Known: designed to add unpredictable, organic, tape-like movement to each voice, affecting oscillator and filter.
Implementation plan until manual confirms:
- Per-voice **flutter/wow**: smoothed random + low-rate sine pitch wobble, independent per voice
- Per-voice **drift**: slow random walk on pitch and cutoff
- All generators independent per voice; one macro amount + per-destination depth
- **[CONFIRM]** exact parameter names, count, and ranges

## Drive + FX
- Up to four distortion points **[CONFIRM locations]**; minimum: per-voice saturation + stereo "CMOS-style" drive.
- Oversample nonlinear stages (2–4×) to control aliasing.
- Stereo reverb (algorithmic; start simple, e.g. FDN).

## Milestones (tag each)
- **v1.1** — CMake JUCE skeleton builds VST3/AU/Standalone; one voice: osc → filter → env → VCA; passes pluginval.
- **v1.2** — 5 fixed voices + Forward round robin; per-voice offsets for tune/morph/level.
- **v1.3** — All 6 allocation modes.
- **v1.4** — Per-voice LFO + looping envelope; randomize system with seed recall.
- **v1.5** — Wildcard layer.
- **v1.6** — Sub-osc, filter modes, drive stages, reverb.
- **v1.7** — MPE; preset save/load.
- **v1.8** — UI pass.
- **v2** — First exported build for testing by others.

## Working rules for Claude Code
- One milestone at a time. Don't start the next until the current one builds, passes pluginval, and is tagged.
- No allocations or locks on the audio thread. Smooth all parameter changes.
- Unit-test the allocator independently of audio (it's pure logic — test every mode).
- When behavior depends on a **[CONFIRM]** item, implement the simplest reasonable version, leave a `// CONFIRM:` comment, and list it in `OPEN_QUESTIONS.md`.
