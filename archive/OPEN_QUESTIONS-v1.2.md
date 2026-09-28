# Open Questions

Things the manual doesn't answer. Resolve by ear, demo videos, hands-on time, or asking the manufacturer.
When one is resolved: move it to "Resolved", note the source and the date, and update CLAUDE.md.

## Voice allocation (priority 1)
- [ ] Round-Robin Random: can the same voice repeat back-to-back? (Assumed: no immediate repeat.)
- [ ] Round-Robin steal: does the next voice take the note even while it's still sounding? (Assumed: yes.)
- [ ] Unison with 2–4 held notes: how are the 5 voices split, and which voice numbers go to which note?
- [ ] Does unison add any detune/spread by itself, or only via the wildcard params?

## Voices / global
- [ ] Voice octave range.
- [ ] Sub octave options.
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

## Resolved
(none yet)
