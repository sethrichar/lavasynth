#pragma once

#include "ControlFold.h"
#include "WildcardTuning.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rotor
{

// Small seeded random source shared by the wildcard processes.
class Random
{
public:
    explicit Random (std::uint32_t seed = 1) : state (seed != 0 ? seed : 1u) {}
    std::uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    double uniform() { return (double) next() / 4294967296.0; }  // 0..1
    double bipolar() { return 2.0 * uniform() - 1.0; }           // -1..1

private:
    std::uint32_t state;
};

// Smooth random: a new random target every period, raised-cosine glide between them.
class SmoothRandom
{
public:
    void setRate (double hz) { rateHz = hz; }

    // Advance by dt seconds; returns -1..1.
    double tick (double dt, Random& rng)
    {
        position += dt * rateHz * jitter;
        while (position >= 1.0)
        {
            position -= 1.0;
            from = to;
            to = rng.bipolar();
            jitter = 0.7 + 0.6 * rng.uniform(); // ±30% so voices drift apart
        }
        const double s = 0.5 - 0.5 * std::cos (3.141592653589793 * position);
        return from + (to - from) * s;
    }

private:
    double rateHz = 1.0;
    double position = 1.0; // pick a first target on the first tick
    double from = 0.0, to = 0.0, jitter = 1.0;
};

// The per-voice wildcard processes. Pure logic: the voice calls tick() at its modulation rate
// and reads the results. Every output is exactly neutral when its amount is 0.
class Wildcards
{
public:
    struct Amounts // each 0..1 (the unipolar panel sliders)
    {
        double noteDetune = 0.0;
        double wow = 0.0;
        double flutter = 0.0;
        double reelDrag = 0.0;
        double chaos = 0.0;
        double envScatter = 0.0;
    };

    explicit Wildcards (std::uint32_t seed = 1) : rng (seed)
    {
        wow.setRate (wildcard::wowRateHz);
        flutter.setRate (wildcard::flutterRateHz);
        chaosVolume.setRate (wildcard::chaosRateHz);
        chaosCutoff.setRate (wildcard::chaosRateHz);
    }

    void setAmounts (const Amounts& a) { amounts = a; }

    // Key press (envelope trigger): new note detune.
    void trigger() { detuneUnit = rng.bipolar(); }

    void tick (double dt)
    {
        const double w = wow.tick (dt, rng);
        const double f = flutter.tick (dt, rng);
        tickReelDrag (dt);

        pitchCents = amounts.noteDetune * wildcard::noteDetuneMaxCents * detuneUnit
                     + amounts.wow * wildcard::wowMaxCents * w
                     + amounts.flutter * wildcard::flutterMaxCents * f
                     - amounts.reelDrag * wildcard::reelDragMaxCents * dragDepth * dragShape;

        // Chaos
        const double v = chaosVolume.tick (dt, rng);
        const double c = chaosCutoff.tick (dt, rng);
        gain = std::pow (10.0, -amounts.chaos * wildcard::chaosMaxDipDb * 0.5 * (v + 1.0) / 20.0);
        cutoffOctaves = amounts.chaos * wildcard::chaosCutoffOctaves * c;
        tickNoiseBurst (dt);
    }

    double getPitchCents() const { return pitchCents; }
    double getGain() const { return gain; }
    double getCutoffOctaves() const { return cutoffOctaves; }
    // Level of the chaos noise burst right now (0 when none).
    double getNoiseBurstLevel() const { return burstRemaining > 0.0 ? amounts.chaos * wildcard::chaosBurstLevel : 0.0; }
    // White noise for the burst (call per audio sample while a burst is on).
    float noiseSample() { return (float) rng.bipolar(); }

    // Envelope scatter: multipliers for the amp envelope's attack, decay and release times,
    // drawn per trigger. Rules from the manual (a stage "at 0" = at its minimum):
    //   attack 0, decay > 0 → decay only; attack > 0, decay 0 → attack only; both 0 → both;
    //   both > 0 → both (OPEN); release always, unless it is at maximum.
    struct Scatter
    {
        double attack = 1.0, decay = 1.0, release = 1.0;
    };

    Scatter drawScatter (bool attackAtMin, bool decayAtMin, bool releaseAtMax)
    {
        Scatter s;
        const double amount = amounts.envScatter;
        if (amount <= 0.0)
            return s;

        // A stage at its minimum can only get longer; others go either way.
        auto factor = [&] (bool atMin)
        {
            const double u = rng.bipolar();
            return std::exp2 ((atMin ? std::abs (u) : u) * amount * wildcard::envScatterMaxOctaves);
        };

        const bool randomizeAttack = ! (attackAtMin && ! decayAtMin);
        const bool randomizeDecay = ! (decayAtMin && ! attackAtMin);
        if (randomizeAttack) s.attack = factor (attackAtMin);
        if (randomizeDecay) s.decay = factor (decayAtMin);
        if (! releaseAtMax) s.release = factor (false);
        return s;
    }

private:
    void tickReelDrag (double dt)
    {
        const double amount = amounts.reelDrag;
        if (dragPhase == 0 && amount > 0.0 && rng.uniform() < amount * wildcard::reelDragBurstsPerSecond * dt)
        {
            dragPhase = 1;
            dragDepth = 0.3 + 0.7 * rng.uniform();
        }
        if (dragPhase == 1) // falling: linear to full depth
        {
            dragShape += dt / wildcard::reelDragFallSeconds;
            if (dragShape >= 1.0)
            {
                dragShape = 1.0;
                dragPhase = 2;
            }
        }
        else if (dragPhase == 2) // recovering: exponential back to pitch
        {
            dragShape *= std::exp (-dt * 4.6 / wildcard::reelDragRecoverSeconds);
            if (dragShape < 0.001)
            {
                dragShape = 0.0;
                dragPhase = 0;
            }
        }
    }

    void tickNoiseBurst (double dt)
    {
        if (burstRemaining > 0.0)
            burstRemaining -= dt;
        else if (amounts.chaos > 0.0 && rng.uniform() < amounts.chaos * wildcard::chaosBurstsPerSecond * dt)
            burstRemaining = wildcard::chaosBurstMinSeconds
                             + rng.uniform() * (wildcard::chaosBurstMaxSeconds - wildcard::chaosBurstMinSeconds);
    }

    Random rng;
    Amounts amounts;
    SmoothRandom wow, flutter, chaosVolume, chaosCutoff;
    double detuneUnit = 0.0;
    double pitchCents = 0.0, gain = 1.0, cutoffOctaves = 0.0;
    int dragPhase = 0;
    double dragShape = 0.0, dragDepth = 0.0;
    double burstRemaining = 0.0;
};

// Stereo spreader: pan position (−1..+1) for each voice.
//   spread     — the slider (0..1) = width; it is the offset the modulation moves.
//   envMod     — mod-env depth × mod-env level (bipolar). It widens/narrows the spread (negative
//                past zero reverses the pan order) and also moves voice 1.
//   lfoMod     — LFO depth × that voice's LFO (bipolar): moves each voice on its own.
// Spread and each pan fold back at their limits (control-signal wavefolding).
// OPEN: how exactly the mod env moves voice 1 (here it pans by the same amount as the spread moves).
inline double spreadPan (int voice, double spread, double envMod, double lfoMod)
{
    auto fold = [] (double x) { return foldIntoRange (x, -1.0, 1.0); };
    const double width = fold (spread + envMod);
    const double base = voice == 0 ? envMod : width * wildcard::spreadPositions[voice];
    return fold (base + lfoMod);
}

// Constant-power pan gains.
inline void panGains (double pan, float& left, float& right)
{
    const double angle = (std::clamp (pan, -1.0, 1.0) + 1.0) * 0.25 * 3.141592653589793;
    left = (float) std::cos (angle);
    right = (float) std::sin (angle);
}

} // namespace rotor
