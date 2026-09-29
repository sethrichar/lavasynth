#pragma once

// Tuning constants for aftertouch, MPE, the mod wheel and Global Detune / Pitch Drift.
// OPEN: all first guesses — tune by ear.
namespace rotor::performance
{

// Aftertouch sliders (bipolar) at full depth and full pressure:
inline constexpr double atCutoffOctaves = 3.0;   // Filter cutoff AT
inline constexpr double atLfoRateOctaves = 3.0;  // LFO rate AT
// Wildcard AT, down = "harmonic clusters": pitch offsets of voices 1–5 (voice 3 stays put).
inline constexpr double clusterSemitones[5] = { -12.0, -5.0, 0.0, 7.0, 12.0 };

// MPE timbre (CC74) → cutoff, ± this many octaves around neutral.
inline constexpr double timbreCutoffOctaves = 2.0;

// Mod wheel in "Pitch LFO" mode: vibrato depth at full wheel.
inline constexpr double pitchLfoMaxSemitones = 1.0;

// Global Detune: ± this many semitones, uniform across voices.
inline constexpr double globalDetuneMaxSemitones = 1.0;

// Pitch Drift: each voice has its own centre note and tracking error; at full amount a note
// one octave from the voice's centre is off by up to this many cents.
inline constexpr double driftMaxCentsPerOctave = 12.0;
inline constexpr double driftCentreNotes[5] = { 48.0, 55.0, 60.0, 67.0, 72.0 };

} // namespace rotor::performance
