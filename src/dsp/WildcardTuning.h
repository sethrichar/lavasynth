#pragma once

// Every tuning constant for the 8 wildcard controls lives here (per CLAUDE.md).
// OPEN: all of these are first guesses from the manual's descriptions — tune by ear.
namespace rotor::wildcard
{

// 1. Note detune: a new random offset per voice on every key press.
inline constexpr double noteDetuneMaxCents = 30.0;

// 2. Wow: slow, smooth random pitch drift per voice.
inline constexpr double wowMaxCents = 35.0;
inline constexpr double wowRateHz = 0.6;          // new random target this often (±30% jitter per voice)

// 3. Flutter: faster pitch wobble per voice.
inline constexpr double flutterMaxCents = 12.0;
inline constexpr double flutterRateHz = 9.0;

// 4. Reel drag: brief random dips in pitch, like a finger dragging on a tape reel.
inline constexpr double reelDragMaxCents = 80.0;
inline constexpr double reelDragBurstsPerSecond = 0.8; // at full amount (random timing)
inline constexpr double reelDragFallSeconds = 0.04;
inline constexpr double reelDragRecoverSeconds = 0.25;

// 5. Chaos: random volume and cutoff movement, plus short noise bursts.
inline constexpr double chaosMaxDipDb = 9.0;      // volume only ever dips (never louder)
inline constexpr double chaosCutoffOctaves = 1.5; // ± around the set cutoff
inline constexpr double chaosRateHz = 4.0;
inline constexpr double chaosBurstsPerSecond = 3.0;
inline constexpr double chaosBurstMinSeconds = 0.01;
inline constexpr double chaosBurstMaxSeconds = 0.06;
inline constexpr double chaosBurstLevel = 0.5;

// 6. Envelope scatter: random amp attack/decay/release per trigger, up to ×/÷ 2^this.
inline constexpr double envScatterMaxOctaves = 2.0;

// 7. Stereo spreader: pan positions at full width (−1 = left, +1 = right).
// Voice 1 centre, 2 and 4 left, 3 and 5 right. OPEN: which pair sits wider.
inline constexpr double spreadPositions[5] = { 0.0, -1.0, 1.0, -0.5, 0.5 };

} // namespace rotor::wildcard
