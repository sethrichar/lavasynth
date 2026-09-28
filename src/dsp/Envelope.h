#pragma once

#include <algorithm>
#include <cmath>

namespace rotor
{

// ADSR with analog-style (RC) curves, loop mode and a rate multiplier for key tracking.
// Used for both the amp and the mod envelope.
//
// Stage times are "full swing" times: attack = time from 0 to 1, decay and release = time
// from 1 to 0. A stage that only has to cover part of the range (decay to a sustain level,
// attack from a loop's sustain level) finishes sooner — so raising sustain speeds up a loop.
//
// Loop: after attack + decay the envelope goes straight back to attack (while the key is
// held). With A, D, S all at minimum the loop runs at C1 = 32.703 Hz.
class Envelope
{
public:
    enum class Stage
    {
        idle,
        attack,
        decay,
        sustain,
        release
    };

    // Fastest loop is C1 (32.703 Hz) per the manual, so minimum attack + minimum decay = 1 / 32.703 s.
    // OPEN: the hardware's actual minimum times (this also sets the shortest non-looping attack).
    static constexpr double fastestLoopHz = 32.703195662574829;
    static constexpr double minTimeSeconds = 0.5 / fastestLoopHz; // ≈ 15.3 ms

    // RC curve targets: attack charges toward 1.2 (stopping at 1), decay/release discharge
    // toward just below 0 so they land exactly on 0.
    static constexpr double attackTarget = 1.2;
    static constexpr double releaseTarget = -0.01;

    struct Parameters
    {
        double attackSeconds = 0.02;
        double decaySeconds = 0.3;
        double sustainLevel = 0.7; // 0..1
        double releaseSeconds = 0.3;
        bool loop = false;
    };

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        updateRates();
    }

    void setParameters (const Parameters& newParams)
    {
        params = newParams;
        params.sustainLevel = std::clamp (params.sustainLevel, 0.0, 1.0);
        updateRates();
    }

    // Multiplies every stage's speed (key tracking: 2 = twice as fast).
    void setRateScale (double scale)
    {
        rateScale = std::clamp (scale, 1.0 / 64.0, 64.0);
        updateRates();
    }

    void noteOn()
    {
        // Retrigger from the current level (no reset to zero) to avoid clicks.
        stage = Stage::attack;
    }

    void noteOff()
    {
        if (stage != Stage::idle)
            stage = Stage::release;
    }

    void reset()
    {
        stage = Stage::idle;
        level = 0.0;
    }

    bool isActive() const { return stage != Stage::idle; }
    Stage getStage() const { return stage; }
    double getLevel() const { return level; }

    float process()
    {
        switch (stage)
        {
            case Stage::idle:
                break;

            case Stage::attack:
                level = attackTarget + (level - attackTarget) * attackCoef;
                if (level >= 1.0)
                {
                    level = 1.0;
                    stage = Stage::decay;
                }
                break;

            case Stage::decay:
                level = releaseTarget + (level - releaseTarget) * decayCoef;
                if (level <= params.sustainLevel)
                {
                    level = params.sustainLevel;
                    // A loop needs somewhere to go: at full sustain it just holds.
                    stage = params.loop && params.sustainLevel < 0.999 ? Stage::attack : Stage::sustain;
                }
                break;

            case Stage::sustain:
                level = params.sustainLevel;
                if (params.loop && params.sustainLevel < 0.999)
                    stage = Stage::attack; // loop switched on while sustaining
                break;

            case Stage::release:
                level = releaseTarget + (level - releaseTarget) * releaseCoef;
                if (level <= 0.0)
                {
                    level = 0.0;
                    stage = Stage::idle;
                }
                break;
        }

        return static_cast<float> (level);
    }

    // Per-sample coefficient for an RC stage that sweeps from `from` to `to` (toward `target`) in `seconds`.
    static double coefficient (double seconds, double sampleRate, double from, double to, double target)
    {
        const double samples = std::max (1.0, seconds * sampleRate);
        return std::pow ((to - target) / (from - target), 1.0 / samples);
    }

private:
    void updateRates()
    {
        auto stageTime = [this] (double seconds) { return std::max (seconds, minTimeSeconds) / rateScale; };
        attackCoef = coefficient (stageTime (params.attackSeconds), sampleRate, 0.0, 1.0, attackTarget);
        decayCoef = coefficient (stageTime (params.decaySeconds), sampleRate, 1.0, 0.0, releaseTarget);
        releaseCoef = coefficient (stageTime (params.releaseSeconds), sampleRate, 1.0, 0.0, releaseTarget);
    }

    double sampleRate = 44100.0;
    Parameters params;
    double rateScale = 1.0;
    Stage stage = Stage::idle;
    double level = 0.0;
    double attackCoef = 0.0;
    double decayCoef = 0.0;
    double releaseCoef = 0.0;
};

// Key tracking for envelope (and LFO) rates. amount -1..+1: positive = higher notes faster,
// negative = lower notes faster; ±1 doubles/halves the rate per octave.
// OPEN: key-tracking range and reference note (middle C assumed).
inline double keyTrackedRate (double pitchSemitones, double amount)
{
    constexpr double referenceNote = 60.0;
    return std::pow (2.0, amount * (pitchSemitones - referenceNote) / 12.0);
}

} // namespace rotor
