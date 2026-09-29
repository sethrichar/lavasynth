#include "TestHelpers.h"
#include "dsp/Drive.h"
#include "dsp/LfoMinMax.h"
#include "dsp/Reverb.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <vector>

namespace
{
    constexpr double sr = 48000.0;
    constexpr double pi = 3.141592653589793;

    struct Stereo
    {
        std::vector<float> l, r;
    };

    Stereo reverbImpulse (double amount, double color, double seconds)
    {
        rotor::PlateReverb rev;
        rev.prepare (sr);
        rev.setParameters (amount, color, 1.0);
        Stereo out;
        for (int i = 0; i < (int) (seconds * sr); ++i)
        {
            float l = i == 0 ? 1.0f : 0.0f, r = l;
            rev.process (l, r);
            out.l.push_back (l);
            out.r.push_back (r);
        }
        return out;
    }

    // Seconds until the energy (50 ms windows) falls 60 dB below its loudest window.
    double rt60 (const std::vector<float>& x)
    {
        const size_t w = (size_t) (0.05 * sr);
        std::vector<double> e;
        for (size_t i = 0; i + w <= x.size(); i += w)
        {
            double s = 0.0;
            for (size_t j = i; j < i + w; ++j) s += (double) x[j] * x[j];
            e.push_back (s);
        }
        const double peak = *std::max_element (e.begin(), e.end());
        for (size_t i = e.size(); i-- > 0;)
            if (e[i] > peak * 1e-6)
                return (i + 1) * 0.05;
        return 0.0;
    }

    double correlation (const std::vector<float>& a, const std::vector<float>& b, size_t from)
    {
        double ab = 0.0, aa = 0.0, bb = 0.0;
        for (size_t i = from; i < a.size(); ++i)
        {
            ab += (double) a[i] * b[i];
            aa += (double) a[i] * a[i];
            bb += (double) b[i] * b[i];
        }
        return ab / std::sqrt (aa * bb);
    }

    double centroidHz (const std::vector<float>& x, size_t from)
    {
        const size_t n = 8192;
        std::vector<std::complex<double>> buf (n);
        for (size_t i = 0; i < n; ++i)
            buf[i] = x[from + i];
        testing::fft (buf);
        double num = 0.0, den = 0.0;
        for (size_t k = 1; k < n / 2; ++k)
        {
            num += k * sr / n * std::norm (buf[k]);
            den += std::norm (buf[k]);
        }
        return num / den;
    }
} // namespace

TEST_CASE ("Reverb: mix 0 passes the input untouched", "[reverb]")
{
    rotor::PlateReverb rev;
    rev.prepare (sr);
    rev.setParameters (0.8, 0.3, 0.0);
    for (int i = 0; i < 4800; ++i)
    {
        const float inL = (float) std::sin (i * 0.05), inR = (float) std::cos (i * 0.031);
        float l = inL, r = inR;
        rev.process (l, r);
        REQUIRE (l == inL);
        REQUIRE (r == inR);
    }
}

TEST_CASE ("Reverb Amount: longer tail and wider image as it rises", "[reverb]")
{
    const auto small = reverbImpulse (0.0, 0.0, 12.0);
    const auto large = reverbImpulse (1.0, 0.0, 12.0);
    const double shortTail = rt60 (small.l), longTail = rt60 (large.l);
    INFO ("RT60 at amount 0: " << shortTail << " s, at amount 1: " << longTail << " s");
    CHECK (shortTail < 2.0);
    CHECK (longTail > 4.0);

    const size_t from = (size_t) (0.1 * sr);
    const double narrow = correlation (small.l, small.r, from);
    const double wide = correlation (large.l, large.r, from);
    INFO ("L/R correlation: amount 0 = " << narrow << ", amount 1 = " << wide);
    CHECK (narrow > 0.99); // amount 0 = mono wet
    CHECK (wide < 0.5);
}

TEST_CASE ("Reverb Color: bright tail is brighter than dark", "[reverb]")
{
    const auto dark = reverbImpulse (0.6, -1.0, 1.0);
    const auto bright = reverbImpulse (0.6, 1.0, 1.0);
    const size_t from = (size_t) (0.2 * sr);
    CHECK (centroidHz (bright.l, from) > 1.5 * centroidHz (dark.l, from));
}

TEST_CASE ("Reverb stays stable at extreme settings", "[reverb]")
{
    const double color = GENERATE (-1.0, 0.0, 1.0);
    rotor::PlateReverb rev;
    rev.prepare (sr);
    rev.setParameters (1.0, color, 1.0);
    float peak = 0.0f;
    std::uint32_t seed = 1;
    for (int i = 0; i < (int) (30 * sr); ++i)
    {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        float l = (float) seed / 4294967296.0f - 0.5f, r = l;
        rev.process (l, r);
        REQUIRE (std::isfinite (l));
        peak = std::max (peak, std::abs (l));
    }
    CHECK (peak < 10.0f);
}

namespace
{
    std::vector<float> driveSine (double amount, double color, double mix, double hz, double amp, int n)
    {
        rotor::CmosDrive d;
        d.prepare (sr);
        d.setParameters (amount, color, mix);
        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; ++i)
            out[(size_t) i] = d.process ((float) (amp * std::sin (2.0 * pi * hz * i / sr)));
        return out;
    }

    // Energy share in harmonic k of a bin-centred sine (k = 1 is the fundamental).
    double harmonicEnergy (const std::vector<float>& x, size_t bin, int k)
    {
        const size_t n = 8192;
        std::vector<std::complex<double>> buf (n);
        for (size_t i = 0; i < n; ++i)
            buf[i] = x[x.size() - n + i];
        testing::fft (buf);
        double total = 0.0;
        for (size_t j = 1; j < n / 2; ++j)
            total += std::norm (buf[j]);
        return std::norm (buf[bin * (size_t) k]) / total;
    }
} // namespace

TEST_CASE ("Drive: mix 0 is the input delayed by the oversampling latency", "[drive]")
{
    const double hz = 1000.0;
    const auto out = driveSine (1.0, 0.0, 0.0, hz, 0.5, 9600);
    const double lat = rotor::CmosDrive::latencySamples();
    double err = 0.0, ref = 0.0;
    for (int i = 2000; i < 9600; ++i)
    {
        const double expected = 0.5 * std::sin (2.0 * pi * hz * (i - lat) / sr);
        err += (out[(size_t) i] - expected) * (out[(size_t) i] - expected);
        ref += expected * expected;
    }
    CHECK (10.0 * std::log10 (err / ref) < -60.0);
}

TEST_CASE ("Drive: more amount = more distortion; asymmetric (even harmonics)", "[drive]")
{
    const size_t bin = 128; // 128 * 48000 / 8192 = 750 Hz
    const double hz = bin * sr / 8192.0;
    const auto light = driveSine (0.2, 0.0, 1.0, hz, 0.3, 24000);
    const auto heavy = driveSine (1.0, 0.0, 1.0, hz, 0.3, 24000);
    CHECK (1.0 - harmonicEnergy (heavy, bin, 1) > 1.0 - harmonicEnergy (light, bin, 1));
    CHECK (harmonicEnergy (heavy, bin, 2) > 3e-3); // CMOS asymmetry → a clear 2nd harmonic
}

TEST_CASE ("Drive: output bounded and finite with loud input", "[drive]")
{
    const double color = GENERATE (-1.0, 0.0, 1.0);
    const auto out = driveSine (1.0, color, 1.0, 110.0, 4.0, 48000);
    for (float y : out)
    {
        REQUIRE (std::isfinite (y));
        REQUIRE (std::abs (y) < 2.0f);
    }
}

TEST_CASE ("Drive Color: centre is flat, off-centre scoops where it points", "[drive]")
{
    // Noise through a clean-ish setting, comparing band energy around the scoop.
    auto bandDb = [] (double color, double lo, double hi)
    {
        rotor::CmosDrive d;
        d.prepare (sr);
        d.setParameters (0.0, color, 1.0);
        std::uint32_t seed = 3;
        const size_t n = 8192;
        std::vector<double> spectrum (n, 0.0);
        std::vector<std::complex<double>> buf (n);
        for (int frame = 0; frame < 16; ++frame)
        {
            for (size_t i = 0; i < n; ++i)
            {
                seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
                buf[i] = d.process (0.05f * ((float) seed / 4294967296.0f - 0.5f));
            }
            testing::fft (buf);
            for (size_t k = 0; k < n; ++k) spectrum[k] += std::norm (buf[k]);
        }
        double e = 0.0;
        for (size_t k = 1; k < n / 2; ++k)
        {
            const double f = k * sr / n;
            if (f >= lo && f < hi) e += spectrum[k];
        }
        return 10.0 * std::log10 (e);
    };
    // Colour +0.6 points the scoop up to ~2.3 kHz: that band drops vs centre; 200 Hz barely moves.
    const double scoopHz = 1000.0 * std::exp2 (0.6 * 2.5);
    CHECK (bandDb (0.6, scoopHz * 0.9, scoopHz * 1.1) < bandDb (0.0, scoopHz * 0.9, scoopHz * 1.1) - 5.0);
    CHECK (std::abs (bandDb (0.6, 150.0, 250.0) - bandDb (0.0, 150.0, 250.0)) < 1.5);
}

TEST_CASE ("LFO min-maxing: up = maximum of the five, down = minimum, centre = off", "[lfo][minmax]")
{
    const std::array<float, 5> lfos { -0.4f, 0.9f, 0.1f, -0.8f, 0.3f };
    CHECK (rotor::lfoMinMax (lfos, 1.0) == Catch::Approx (0.9));
    CHECK (rotor::lfoMinMax (lfos, 0.5) == Catch::Approx (0.45));
    CHECK (rotor::lfoMinMax (lfos, -1.0) == Catch::Approx (-0.8));
    CHECK (rotor::lfoMinMax (lfos, -0.25) == Catch::Approx (-0.2));
    CHECK (rotor::lfoMinMax (lfos, 0.0) == 0.0);
}
