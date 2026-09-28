#include "TestHelpers.h"
#include "dsp/Oscillator.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using rotor::Oscillator;
using rotor::Waveform;

namespace
{
    constexpr double sr = 48000.0;

    std::vector<float> render (Waveform w, double hz, size_t n)
    {
        Oscillator osc;
        osc.setSampleRate (sr);
        osc.setWaveform (w);
        osc.setFrequency (hz);
        std::vector<float> out (n);
        for (auto& s : out)
            s = osc.process();
        return out;
    }

    // Same waveform with no band-limiting: phase ramp shaped directly.
    std::vector<float> renderNaive (Waveform w, double hz, size_t n)
    {
        std::vector<float> out (n);
        double t = 0.0;
        const double dt = hz / sr;
        for (auto& s : out)
        {
            switch (w)
            {
                case Waveform::square: s = t < 0.5 ? 1.0f : -1.0f; break;
                case Waveform::saw: s = (float) (2.0 * t - 1.0); break;
                case Waveform::sharkTooth:
                {
                    const double r = Oscillator::sharkToothRise;
                    s = (float) (t < r ? -1.0 + 2.0 * t / r : 1.0 - 2.0 * (t - r) / (1.0 - r));
                    break;
                }
                case Waveform::triangle: s = (float) (t < 0.5 ? -1.0 + 4.0 * t : 3.0 - 4.0 * t); break;
                case Waveform::sine: s = (float) std::sin (2.0 * 3.141592653589793 * t); break;
            }
            t += dt;
            if (t >= 1.0) t -= 1.0;
        }
        return out;
    }
} // namespace

TEST_CASE ("All waveforms stay within a sane range and have no DC offset", "[oscillator]")
{
    const auto w = GENERATE (Waveform::square, Waveform::saw, Waveform::sharkTooth, Waveform::triangle, Waveform::sine);
    const double hz = GENERATE (27.5, 440.0, 4186.0);

    // Whole number of cycles-ish: long buffer makes the mean converge.
    const auto x = render (w, hz, 48000);
    const auto [lo, hi] = std::minmax_element (x.begin(), x.end());
    CHECK (*hi <= 1.2f);
    CHECK (*lo >= -1.2f);
    CHECK (*hi >= 0.8f);
    CHECK (*lo <= -0.8f);

    double mean = 0.0;
    for (float s : x) mean += s;
    mean /= (double) x.size();
    CHECK (std::abs (mean) < 0.02);
}

TEST_CASE ("Band-limiting reduces aliasing versus the naive waveform", "[oscillator]")
{
    // Non-integer ratio to sample rate so aliases land between harmonics.
    const double hz = 3001.7;
    const size_t n = 1 << 15;
    const auto w = GENERATE (Waveform::square, Waveform::saw, Waveform::sharkTooth, Waveform::triangle);

    const double bandLimited = testing::aliasToHarmonicDb (render (w, hz, n), hz, sr);
    const double naive = testing::aliasToHarmonicDb (renderNaive (w, hz, n), hz, sr);
    INFO ("waveform " << (int) w << ": band-limited " << bandLimited << " dB, naive " << naive << " dB");
    CHECK (bandLimited < naive - 6.0);
}

TEST_CASE ("Sine has essentially no aliasing", "[oscillator]")
{
    const double hz = 3001.7;
    CHECK (testing::aliasToHarmonicDb (render (Waveform::sine, hz, 1 << 15), hz, sr) < -80.0);
}

TEST_CASE ("Oscillator frequency matches the requested pitch", "[oscillator]")
{
    // Count rising zero crossings of the sine over one second.
    const auto x = render (Waveform::sine, 440.0, 48000);
    int crossings = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            ++crossings;
    CHECK (std::abs (crossings - 440) <= 1);
}
