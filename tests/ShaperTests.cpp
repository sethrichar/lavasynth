#include "TestHelpers.h"
#include "dsp/Shapers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace rotor;

namespace
{
    // Share of energy above the fundamental of a shaped 1 kHz sine (0 = pure sine).
    double harmonicShare (float (*shape) (float, double), double param, double amp)
    {
        const size_t n = 4096, fundamental = 85; // power-of-two FFT, fundamental on a bin (~996 Hz)
        const double sr = 48000.0, hz = fundamental * sr / n;
        std::vector<std::complex<double>> buf (n);
        for (size_t i = 0; i < n; ++i)
            buf[i] = shape ((float) (amp * std::sin (2.0 * 3.141592653589793 * hz * i / sr)), param);
        testing::fft (buf);
        double total = 0.0;
        for (size_t k = 1; k < n / 2; ++k)
            total += std::norm (buf[k]);
        return 1.0 - std::norm (buf[fundamental]) / total;
    }

    // Spectral centroid in harmonics of the fundamental (1 = pure sine); higher = brighter.
    double brightness (float (*shape) (float, double), double param, double amp)
    {
        const size_t n = 4096, fundamental = 85;
        std::vector<std::complex<double>> buf (n);
        for (size_t i = 0; i < n; ++i)
            buf[i] = shape ((float) (amp * std::sin (2.0 * 3.141592653589793 * (double) (fundamental * i) / n)), param);
        testing::fft (buf);
        double weighted = 0.0, total = 0.0;
        for (size_t k = 1; k < n / 2; ++k)
        {
            weighted += (double) k * std::norm (buf[k]);
            total += std::norm (buf[k]);
        }
        return weighted / total / (double) fundamental;
    }
} // namespace

TEST_CASE ("OSC Level: clean gain below the midpoint, drive above", "[shapers]")
{
    CHECK (OscLevel::mainGain (0.0) == 0.0);
    CHECK (OscLevel::mainGain (0.25) == Catch::Approx (0.5));
    CHECK (OscLevel::mainGain (0.5) == 1.0);
    CHECK (OscLevel::mainGain (1.0) == 1.0);
    CHECK (OscLevel::drive (0.5) == 0.0);
    CHECK (OscLevel::drive (0.75) == Catch::Approx (0.5));
    CHECK (OscLevel::drive (1.0) == 1.0);
}

TEST_CASE ("Overdrive: bypass at zero drive, more drive = more harmonics, bounded", "[shapers]")
{
    for (float x : { -1.0f, -0.3f, 0.0f, 0.7f })
        CHECK (oscOverdrive (x, 0.0) == x);

    const double light = harmonicShare (oscOverdrive, 0.3, 0.9);
    const double heavy = harmonicShare (oscOverdrive, 1.0, 0.9);
    CHECK (light > 0.001);
    CHECK (heavy > light);

    for (float x = -3.0f; x <= 3.0f; x += 0.01f)
        REQUIRE (std::abs (oscOverdrive (x, 1.0)) <= 1.0f);
}

TEST_CASE ("Wavefolder: bypass at zero, adds harmonics, output bounded", "[shapers]")
{
    for (float x : { -1.5f, -0.3f, 0.0f, 0.7f })
        CHECK (wavefold (x, 0.0) == x);

    CHECK (harmonicShare (wavefold, 0.1, 0.9) > 0.01);

    // More fold = brighter (energy moves to higher harmonics).
    const double b1 = brightness (wavefold, 0.1, 0.9);
    const double b2 = brightness (wavefold, 0.5, 0.9);
    const double b3 = brightness (wavefold, 1.0, 0.9);
    INFO ("brightness " << b1 << ", " << b2 << ", " << b3);
    CHECK (b1 > 1.0);
    CHECK (b2 > b1);
    CHECK (b3 > b2);

    for (float x = -1.0f; x <= 1.0f; x += 0.01f)
        REQUIRE (std::abs (wavefold (x, 1.0)) <= 1.0f + 1e-6f);
}

TEST_CASE ("fastTanh matches std::tanh", "[shapers]")
{
    double worst = 0.0;
    for (double x = -8.0; x <= 8.0; x += 0.001)
        worst = std::max (worst, std::abs (fastTanh (x) - std::tanh (x)));
    CHECK (worst < 1e-4);
    for (double x = -3.0; x <= 3.0; x += 0.001)
        REQUIRE (std::abs (fastTanh (x) - std::tanh (x)) < 1e-6);
}
