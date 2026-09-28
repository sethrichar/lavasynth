#include "dsp/LowpassFilter.h"
#include "dsp/Oscillator.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace
{
    constexpr double sr = 48000.0;

    double rmsThroughFilter (double sineHz, double cutoffHz, double res)
    {
        rotor::LowpassFilter f;
        f.setSampleRate (sr);
        f.setParameters (cutoffHz, res);
        rotor::Oscillator osc;
        osc.setSampleRate (sr);
        osc.setWaveform (rotor::Waveform::sine);
        osc.setFrequency (sineHz);

        double sum = 0.0;
        const int settle = 4800, n = 48000;
        for (int i = 0; i < settle + n; ++i)
        {
            const float y = f.process (osc.process());
            if (i >= settle)
                sum += (double) y * y;
        }
        return std::sqrt (sum / n);
    }
} // namespace

TEST_CASE ("Lowpass passes lows and cuts highs", "[filter]")
{
    const double inputRms = 1.0 / std::sqrt (2.0);
    CHECK (rmsThroughFilter (100.0, 2000.0, 0.0) > 0.9 * inputRms);
    // Two octaves above cutoff: 12 dB/oct → about -24 dB.
    CHECK (rmsThroughFilter (8000.0, 2000.0, 0.0) < 0.1 * inputRms);
}

TEST_CASE ("Resonance boosts the cutoff region", "[filter]")
{
    CHECK (rmsThroughFilter (1000.0, 1000.0, 0.8) > 2.0 * rmsThroughFilter (1000.0, 1000.0, 0.0));
}

TEST_CASE ("Filter stays stable at extreme settings", "[filter]")
{
    rotor::LowpassFilter f;
    f.setSampleRate (sr);
    f.setParameters (30000.0, 1.0); // above Nyquist → clamped
    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = f.process ((i % 100) < 50 ? 1.0f : -1.0f);
        REQUIRE (std::isfinite (y));
        peak = std::max (peak, std::abs (y));
    }
    CHECK (peak < 100.0f);
}
