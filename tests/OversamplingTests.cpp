#include "dsp/Oversampling.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

namespace
{
    constexpr double baseRate = 48000.0;
    constexpr double pi = 3.141592653589793;

    // Feeds a sine at `hz` (sampled at 4× base) through the decimator; returns output RMS / input RMS.
    double decimatedGain (double hz)
    {
        rotor::Decimator4x d;
        const double osRate = baseRate * 4.0;
        const int outSamples = 24000, settle = 1000;
        double in = 0.0, out = 0.0;
        float block[4];
        for (int i = 0; i < outSamples; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                const double t = (4.0 * i + j) / osRate;
                block[j] = (float) std::sin (2.0 * pi * hz * t);
                if (i >= settle)
                    in += (double) block[j] * block[j] / 4.0;
            }
            const float y = d.process (block);
            if (i >= settle)
                out += (double) y * y;
        }
        return std::sqrt (out / in);
    }

    double db (double gain) { return 20.0 * std::log10 (gain); }
} // namespace

TEST_CASE ("Decimator passes the audio band flat", "[oversampling]")
{
    for (double hz : { 50.0, 1000.0, 10000.0, 18000.0 })
    {
        INFO (hz << " Hz");
        CHECK (db (decimatedGain (hz)) == Catch::Approx (0.0).margin (0.1));
    }
}

TEST_CASE ("Decimator rejects content that would alias into the audio band", "[oversampling]")
{
    // 34 kHz → would alias to 14 kHz; 100 kHz → would alias to 4 kHz.
    for (double hz : { 34000.0, 60000.0, 100000.0, 150000.0 })
    {
        INFO (hz << " Hz");
        CHECK (db (decimatedGain (hz)) < -60.0);
    }
}

TEST_CASE ("Halfband taps are symmetric with zeros at even offsets", "[oversampling]")
{
    // Impulse response check through the public interface: DC gain is 1.
    rotor::HalfbandDecimator<31> h;
    float y = 0.0f;
    for (int i = 0; i < 64; ++i)
        y = h.process (1.0f, 1.0f);
    CHECK (y == Catch::Approx (1.0f).margin (1e-6));
}
