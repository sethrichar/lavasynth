#pragma once

#include <algorithm>
#include <cmath>

namespace rotor
{

// ADSR: linear attack, exponential decay and release.
// v1.5 adds loop mode, key tracking and host sync.
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

    // OPEN: minimum stage times and slider curves. 1 ms keeps note starts click-free.
    static constexpr double minTimeSeconds = 0.001;

    struct Parameters
    {
        double attackSeconds = 0.005;
        double decaySeconds = 0.3;
        double sustainLevel = 0.7; // 0..1
        double releaseSeconds = 0.3;
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
                level += attackStep;
                if (level >= 1.0)
                {
                    level = 1.0;
                    stage = Stage::decay;
                }
                break;

            case Stage::decay:
                level = params.sustainLevel + (level - params.sustainLevel) * decayCoef;
                if (level - params.sustainLevel < settleThreshold)
                {
                    level = params.sustainLevel;
                    stage = Stage::sustain;
                }
                break;

            case Stage::sustain:
                level = params.sustainLevel;
                break;

            case Stage::release:
                level *= releaseCoef;
                if (level < settleThreshold)
                {
                    level = 0.0;
                    stage = Stage::idle;
                }
                break;
        }

        return static_cast<float> (level);
    }

private:
    // Exponential stages reach ~-60 dB of their distance in the stated time.
    static constexpr double settleThreshold = 0.001;

    double coefFor (double seconds) const
    {
        const double samples = std::max (seconds, minTimeSeconds) * sampleRate;
        return std::exp (std::log (settleThreshold) / samples);
    }

    void updateRates()
    {
        attackStep = 1.0 / (std::max (params.attackSeconds, minTimeSeconds) * sampleRate);
        decayCoef = coefFor (params.decaySeconds);
        releaseCoef = coefFor (params.releaseSeconds);
    }

    double sampleRate = 44100.0;
    Parameters params;
    Stage stage = Stage::idle;
    double level = 0.0;
    double attackStep = 0.0;
    double decayCoef = 0.0;
    double releaseCoef = 0.0;
};

} // namespace rotor
