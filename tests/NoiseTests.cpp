#include "TestHelpers.h"
#include "dsp/Noise.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <complex>

namespace
{
    constexpr double sr = 48000.0;

    // Energy per octave band from an averaged spectrum.
    double bandEnergy (const std::vector<double>& spectrum, double lo, double hi, size_t n)
    {
        double e = 0.0;
        for (size_t k = 1; k < n / 2; ++k)
        {
            const double f = k * sr / n;
            if (f >= lo && f < hi)
                e += spectrum[k];
        }
        return e;
    }

    template <typename Source>
    std::vector<double> averagedSpectrum (Source&& next, size_t n, int frames)
    {
        std::vector<double> spectrum (n, 0.0);
        std::vector<std::complex<double>> buf (n);
        for (int f = 0; f < frames; ++f)
        {
            for (size_t i = 0; i < n; ++i)
                buf[i] = next();
            testing::fft (buf);
            for (size_t k = 0; k < n; ++k)
                spectrum[k] += std::norm (buf[k]);
        }
        return spectrum;
    }
} // namespace

TEST_CASE ("Pink noise has roughly equal energy per octave", "[noise]")
{
    rotor::PinkNoise noise;
    const size_t n = 4096;
    const auto spectrum = averagedSpectrum ([&] { return (double) noise.process(); }, n, 64);
    const double low = bandEnergy (spectrum, 200.0, 400.0, n);
    const double high = bandEnergy (spectrum, 6400.0, 12800.0, n);
    // White noise would differ by 15 dB here.
    CHECK (std::abs (10.0 * std::log10 (high / low)) < 2.0);
}

TEST_CASE ("Pink noise level is sane and centred", "[noise]")
{
    rotor::PinkNoise noise;
    double sum = 0.0, sq = 0.0;
    const int n = 480000;
    for (int i = 0; i < n; ++i)
    {
        const double x = noise.process();
        REQUIRE (std::abs (x) < 2.0);
        sum += x;
        sq += x * x;
    }
    CHECK (std::abs (sum / n) < 0.05);
    const double rms = std::sqrt (sq / n);
    CHECK (rms > 0.1);
    CHECK (rms < 0.5);
}

TEST_CASE ("Tilt EQ: flat at centre, tilts around 800 Hz", "[noise]")
{
    auto gainAt = [] (double hz, double color)
    {
        rotor::TiltEq eq;
        eq.setSampleRate (sr);
        eq.setColor (color);
        double in = 0.0, out = 0.0;
        for (int i = 0; i < 96000; ++i)
        {
            const double x = std::sin (2.0 * 3.141592653589793 * hz * i / sr);
            const double y = eq.process ((float) x);
            if (i > 48000)
            {
                in += x * x;
                out += y * y;
            }
        }
        return 10.0 * std::log10 (out / in);
    };

    CHECK (gainAt (100.0, 0.0) == Catch::Approx (0.0).margin (0.1));
    CHECK (gainAt (8000.0, 0.0) == Catch::Approx (0.0).margin (0.1));

    // Full bright: lows cut, highs boosted, by about the per-side maximum.
    CHECK (gainAt (50.0, 1.0) == Catch::Approx (-6.0).margin (1.0));
    CHECK (gainAt (15000.0, 1.0) == Catch::Approx (6.0).margin (1.0));
    // Full dark mirrors it.
    CHECK (gainAt (50.0, -1.0) == Catch::Approx (6.0).margin (1.0));
    CHECK (gainAt (15000.0, -1.0) == Catch::Approx (-6.0).margin (1.0));
}
