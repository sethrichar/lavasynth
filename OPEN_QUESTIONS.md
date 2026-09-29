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

- [ ] OSC Level overdrive: curve and maximum drive. (Assumed: clean below 50%; above, crossfade into tanh with up to +14 dB drive.)
- [ ] Noise Color tilt range. (Assumed: ±6 dB per side at full Color, i.e. 12 dB end to end.)
- [ ] Is the noise one shared source for all voices or independent per voice? (Assumed: one shared source, like EXT.)

## Filter / wavefolder
- [ ] Bandpass slope: the manual's "36 dB/oct bandpass". (Assumed: ladder bandpass, 12 dB/oct each side, + 15% 12 dB lowpass.)
- [ ] Key-tracking range and reference note. (Assumed: 0–100%, 100% = 1:1 tracking, cutoff knob = cutoff at middle C.)
- [ ] Resonance overdrive amount and bandpass vs lowpass loudness at high resonance.
- [ ] Wavefolder depth and curve. (Assumed: sine folder, up to ~3.5 folds on a full-scale input.)

## Modulation (priority 2)
- [ ] Envelope minimum times and slider curves. (v1.5 assumes min attack = min decay = 15.3 ms so the fastest loop is exactly C1 = 32.7 Hz,
  as the manual states; exponential sliders; attack max 20 s, decay max 60 s, release max 1 h. This makes the shortest attack 15 ms.)
- [ ] Envelope curve shape. (Assumed: analog RC curves; stage times are full-swing times, so higher sustain shortens decay and loops.)
- [ ] Envelope/LFO key-tracking reference note. (Assumed: middle C = the panel times; ±1 = double/halve per octave.)
- [ ] Mod envelope depth ranges. (Assumed: cutoff ±5 octaves at full depth; wavefolder ±1 of its range, folding at the ends.)
- [ ] ENV CLK note grid. (Assumed: 1/64 note to 16 bars, straight/triplet/dotted; a 1-hour release snaps to the longest value.)
- [ ] How much sustain shortens A/D in loop mode.
- [ ] Phase distortion algorithm. (Assumed: CZ-style single breakpoint warp, clamped to ±95%.) Is there a PD base knob,
  or is PD modulation-only? (Plugin adds an offset knob, default 0.)
- [ ] Key-tracking knob ranges (amp env, mod env, filter, LFO).
- [ ] Volcano LFO slew amount. (Assumed: raised-cosine glide over each half cycle — maximally smooth.)
- [ ] LFO depth ranges. (Assumed: cutoff ±4 octaves; PD/wavefolder ±1 of range, folding.) Mod env → LFO rate: ±4 octaves.
- [ ] LFO key-tracking reference note. (Assumed: middle C = the panel rate.)
- [ ] LFO sync: does it lock phase to the host's bar position, or only snap the rate? (Assumed: rate only.)
- [ ] LFO starting phase per voice when free-running. (Assumed: all start at 0 when the plugin loads, then drift apart.)

## Wildcards (priority 3)
All constants live in `src/dsp/WildcardTuning.h`; tune by ear.
- [ ] Depth ranges and rates for note detune, wow, flutter, reel drag. (Assumed: detune ±30 ct; wow ±35 ct at ~0.6 Hz;
  flutter ±12 ct at ~9 Hz; reel drag dips up to −80 ct, ~0.8 bursts/s, 40 ms fall, 250 ms recovery.)
- [ ] Reel drag: burst frequency, duration, direction (down only?).
- [ ] Chaos: noise burst character and frequency; volume/cutoff mod rate. (Assumed: volume dips up to −9 dB and cutoff
  ±1.5 oct at ~4 Hz; white-noise bursts 10–60 ms, ~3/s at full.)
- [ ] Envelope scatter when attack AND decay are both > 0.
- [ ] Spreader: pan amounts per voice. (Assumed at full width: voice 2 −1, 3 +1, 4 −0.5, 5 +0.5, voice 1 centre.)
- [ ] Spreader + mod env: how voice 1 moves. (Assumed: voice 1 pans by the mod env amount directly.)
- [ ] Envelope scatter range. (Assumed: up to ×/÷ 4 per stage; a stage at its minimum only gets longer.)
- [ ] Aftertouch "harmonic clusters": what intervals voices 1, 2, 4, 5 move to.

## Wildcards / controls added from cheat sheet
- [ ] Mod Wheel option: which wildcard params does "Katla" mode blend in, and how deep? Pitch LFO depth?
- [ ] Which Master-section toggles (EXT, ENV CLK, LFO CLK) are stored in presets.

## Resolved
- 2026-09-29 — Voicing: keep all Envelope Curves and Filter Characters as permanent selectors, each a button in the final UI. Source: owner.
- 2026-09-28 — Glide source: each voice glides from its own previous note. Source: owner.
- 2026-09-28 — Round-Robin rotation feel (v1.2) approved by ear. Source: owner.
- 2026-09-28 — Voice octave range: −2 to +2, 5 steps. Source: cheat sheet.
- 2026-09-28 — Sub octave: −1 or −2. Source: cheat sheet.
- 2026-09-28 — Audio path order confirmed (osc → filter → wavefolder → amp → voice level → reverb → drive). Source: cheat sheet.
- 2026-09-28 — Wildcard sliders are unipolar; LFO/mod env/aftertouch depths, noise Color, and Effects Color are bipolar. Source: cheat sheet.
