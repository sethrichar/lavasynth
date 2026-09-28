#include "dsp/LadderFilter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>

using rotor::LadderFilter;
using Mode = LadderFilter::Mode;

namespace
{
    constexpr double sr = 192000.0; // voices run 4× oversampled
    constexpr double pi = 3.141592653589793;

    // Steady-state gain (dB) for a sine at `hz` (small level, so the tanh stays linear).
    double gainDb (double hz, double cutoff, double res, Mode mode)
    {
        LadderFilter f;
        f.setSampleRate (sr);
        f.setParameters (cutoff, res, mode);
        const double amp = 0.05;
        const int settle = (int) sr / 5, n = (int) sr / 5;
        double in = 0.0, out = 0.0;
        for (int i = 0; i < settle + n; ++i)
        {
            const double x = amp * std::sin (2.0 * pi * hz * i / sr);
            const float y = f.process ((float) x);
            if (i >= settle)
            {
                in += x * x;
                out += (double) y * y;
            }
        }
        return 10.0 * std::log10 (out / in);
    }

    // Lets the filter ring from an impulse and measures its oscillation frequency.
    double selfOscillationHz (double cutoff, Mode mode, double* rmsOut = nullptr)
    {
        LadderFilter f;
        f.setSampleRate (sr);
        f.setParameters (cutoff, 1.0, mode);
        f.process (1.0f);
        const int settle = (int) sr; // 1 s to reach steady state
        for (int i = 0; i < settle; ++i)
            f.process (0.0f);

        // Count rising zero crossings with sub-sample interpolation over 1 s.
        const int n = (int) sr;
        float prev = f.process (0.0f);
        double first = -1.0, last = -1.0, rms = 0.0;
        int crossings = 0;
        for (int i = 1; i < n; ++i)
        {
            const float y = f.process (0.0f);
            rms += (double) y * y;
            if (prev < 0.0f && y >= 0.0f)
            {
                const double t = (i - 1) + prev / (prev - y);
                if (first < 0.0) first = t;
                last = t;
                ++crossings;
            }
            prev = y;
        }
        if (rmsOut != nullptr)
            *rmsOut = std::sqrt (rms / n);
        return crossings > 1 ? (crossings - 1) * sr / (last - first) : 0.0;
    }
} // namespace

TEST_CASE ("Ladder lowpass: flat passband, 24 dB/oct rolloff", "[ladder]")
{
    const double passband = gainDb (100.0, 2000.0, 0.0, Mode::lowpass);
    CHECK (passband == Catch::Approx (0.0).margin (0.5));

    // Octaves 2 and 3 above the cutoff: close to 24 dB apart.
    const double twoOct = gainDb (8000.0, 2000.0, 0.0, Mode::lowpass);
    const double threeOct = gainDb (16000.0, 2000.0, 0.0, Mode::lowpass);
    CHECK (twoOct < -40.0);
    CHECK (twoOct - threeOct == Catch::Approx (24.0).margin (2.0));
}

TEST_CASE ("Ladder lowpass: resonance peaks at the cutoff", "[ladder]")
{
    const double flat = gainDb (1000.0, 1000.0, 0.0, Mode::lowpass);
    const double resonant = gainDb (1000.0, 1000.0, 0.85, Mode::lowpass);
    CHECK (resonant > flat + 10.0);
}

TEST_CASE ("Ladder bandpass: peaks at the cutoff; lows sit at the lowpass-blend level", "[ladder]")
{
    const double centre = gainDb (1000.0, 1000.0, 0.0, Mode::bandpass);
    CHECK (centre == Catch::Approx (0.0).margin (3.0));
    // Far below the cutoff only the small 12 dB lowpass blend remains.
    const double blendDb = 20.0 * std::log10 (LadderFilter::bandpassLowpassBlend);
    CHECK (gainDb (30.0, 1000.0, 0.0, Mode::bandpass) == Catch::Approx (blendDb).margin (1.0));
    CHECK (gainDb (250.0, 1000.0, 0.0, Mode::bandpass) < centre - 8.0);
    // The small lowpass blend limits the high side's depth; still well down.
    CHECK (gainDb (16000.0, 1000.0, 0.0, Mode::bandpass) < centre - 30.0);
}

TEST_CASE ("Ladder self-oscillates in tune at maximum resonance", "[ladder]")
{
    const double cutoff = GENERATE (110.0, 261.63, 880.0, 3520.0);
    const auto mode = GENERATE (Mode::lowpass, Mode::bandpass);
    double rms = 0.0;
    const double hz = selfOscillationHz (cutoff, mode, &rms);
    INFO ("cutoff " << cutoff << " Hz → oscillates at " << hz << " Hz, rms " << rms);
    CHECK (rms > 0.05);
    // Within 10 cents.
    CHECK (std::abs (1200.0 * std::log2 (hz / cutoff)) < 10.0);
}

TEST_CASE ("Ladder stays bounded and finite with hot input at extreme settings", "[ladder]")
{
    const auto mode = GENERATE (Mode::lowpass, Mode::bandpass);
    LadderFilter f;
    f.setSampleRate (sr);
    f.setParameters (1e6, 1.0, mode); // clamped below Nyquist
    float peak = 0.0f;
    for (int i = 0; i < 96000; ++i)
    {
        const float x = (i % 50) < 25 ? 8.0f : -8.0f;
        const float y = f.process (x);
        REQUIRE (std::isfinite (y));
        peak = std::max (peak, std::abs (y));
    }
    CHECK (peak < 20.0f);
}

TEST_CASE ("Key tracking", "[ladder]")
{
    using rotor::keyTrackedCutoff;
    CHECK (keyTrackedCutoff (1000.0, 72.0, 0.0) == Catch::Approx (1000.0));
    CHECK (keyTrackedCutoff (1000.0, 72.0, 1.0) == Catch::Approx (2000.0));
    CHECK (keyTrackedCutoff (1000.0, 48.0, 1.0) == Catch::Approx (500.0));
    CHECK (keyTrackedCutoff (1000.0, 72.0, 0.5) == Catch::Approx (std::sqrt (2.0) * 1000.0));
    // At full tracking with the cutoff on middle C's pitch, the cutoff lands on each note.
    CHECK (keyTrackedCutoff (261.6256, 69.0, 1.0) == Catch::Approx (440.0).epsilon (1e-4));
}
