# Open Questions

Things the manual doesn't answer. Resolve by ear, demo videos, hands-on time, or asking the manufacturer.
When one is resolved: move it to "Resolved", note the source and the date, and update CLAUDE.md.

## Voice allocation (priority 1)
- [ ] Round-Robin Random: can the same voice repeat back-to-back? (Assumed: no immediate repeat.)
- [ ] Round-Robin steal: does the next voice take the note even while it's still sounding? (Assumed: yes.)
- [ ] Round-Robin Reset in Backward mode: does it return to voice 1 (then 5, 4, …) or to voice 5? (Assumed: voice 1, as the manual says.)
- [ ] Round-Robin Reset in Random mode: any effect? (Assumed: none.)
- [ ] Turning a voice off while it sounds: release normally or cut immediately? (Assumed: normal release.)
- [ ] Unison with 2–4 held notes: how are the 5 voices split, and which voice numbers go to which note?
  (Assumed: voices in panel order take the held notes in press order, cycling — 2 notes → voices 1,3,5 on the first note, 2,4 on the second.)
- [ ] Unison Staccato: when one key of several is lifted and voices move to the remaining notes, do they retrigger? (Assumed: no, they just move.)
- [ ] Mono: does every new key retrigger, or only when the sounding note changes? (Assumed: only when it changes.)
- [ ] Unison Grace Period: how long is it? (Assumed: 80 ms — `VoiceAllocator::defaultGraceSeconds`.)
- [ ] Does unison add any detune/spread by itself, or only via the wildcard params?

## Voices / global
- [ ] Exact shark-tooth waveform shape. (v1.1 assumes a skewed triangle: 85% rise, 15% fall — `Oscillator::sharkToothRise`.)
- [ ] Does the hardware respond to note velocity (amp level, filter)? (v1.1 assumes velocity scales amp level.)
- [ ] How voice level/octave "influence the sub-oscillators, filters, LFO, all key-tracking behaviors" beyond the obvious.
- [ ] Confirm the four distortion stages (assumed: OSC-level overdrive, filter resonance, wavefolder, CMOS drive).

- [ ] Glide curve and knob range. (Assumed: linear in pitch, knob = 0–5 s per octave.)

## Modulation (priority 2)
- [ ] Envelope minimum times and slider curves. (v1.1 assumes 1 ms minimum, exponential sliders; attack max 20 s, decay max 60 s, release max 1 h.)
- [ ] How much sustain shortens A/D in loop mode.
- [ ] Phase distortion algorithm (CZ-style assumed).
- [ ] Key-tracking knob ranges (amp env, mod env, filter, LFO).
- [ ] Volcano LFO slew amount.

## Wildcards (priority 3)
- [ ] Depth ranges and rates for note detune, wow, flutter, reel drag.
- [ ] Reel drag: burst frequency, duration, direction (down only?).
- [ ] Chaos: noise burst character and frequency; volume/cutoff mod rate.
- [ ] Envelope scatter when attack AND decay are both > 0.
- [ ] Spreader: pan amounts per voice.
- [ ] Aftertouch "harmonic clusters": what intervals voices 1, 2, 4, 5 move to.

## Wildcards / controls added from cheat sheet
- [ ] Mod Wheel option: which wildcard params does "Katla" mode blend in, and how deep? Pitch LFO depth?
- [ ] Which Master-section toggles (EXT, ENV CLK, LFO CLK) are stored in presets.

## Resolved
- 2026-09-28 — Glide source: each voice glides from its own previous note. Source: owner.
- 2026-09-28 — Round-Robin rotation feel (v1.2) approved by ear. Source: owner.
- 2026-09-28 — Voice octave range: −2 to +2, 5 steps. Source: cheat sheet.
- 2026-09-28 — Sub octave: −1 or −2. Source: cheat sheet.
- 2026-09-28 — Audio path order confirmed (osc → filter → wavefolder → amp → voice level → reverb → drive). Source: cheat sheet.
- 2026-09-28 — Wildcard sliders are unipolar; LFO/mod env/aftertouch depths, noise Color, and Effects Color are bipolar. Source: cheat sheet.
