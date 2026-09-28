# Open Questions

Things the manual doesn't answer. Resolve by ear, demo videos, hands-on time, or asking the manufacturer.
When one is resolved: move it to "Resolved", note the source and the date, and update CLAUDE.md.

## Voice allocation (priority 1)
- [ ] Round-Robin Random: can the same voice repeat back-to-back? (Assumed: no immediate repeat.)
- [ ] Round-Robin steal: does the next voice take the note even while it's still sounding? (Assumed: yes.)
- [ ] Unison with 2–4 held notes: how are the 5 voices split, and which voice numbers go to which note?
- [ ] Does unison add any detune/spread by itself, or only via the wildcard params?

## Voices / global
- [ ] Exact shark-tooth waveform shape.
- [ ] How voice level/octave "influence the sub-oscillators, filters, LFO, all key-tracking behaviors" beyond the obvious.
- [ ] Confirm the four distortion stages (assumed: OSC-level overdrive, filter resonance, wavefolder, CMOS drive).

## Modulation (priority 2)
- [ ] Envelope minimum times and slider curves.
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
- 2026-09-28 — Voice octave range: −2 to +2, 5 steps. Source: cheat sheet.
- 2026-09-28 — Sub octave: −1 or −2. Source: cheat sheet.
- 2026-09-28 — Audio path order confirmed (osc → filter → wavefolder → amp → voice level → reverb → drive). Source: cheat sheet.
- 2026-09-28 — Wildcard sliders are unipolar; LFO/mod env/aftertouch depths, noise Color, and Effects Color are bipolar. Source: cheat sheet.
