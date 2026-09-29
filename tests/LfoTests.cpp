#include "TestHelpers.h"
#include "dsp/Lfo.h"
#include "dsp/Oscillator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>

using rotor::Lfo;
using Shape = Lfo::Shape;

namespace
{
    constexpr double sr = 48000.0;

    Lfo make (Shape shape, double hz, std::uint32_t seed = 1)
    {
        Lfo l (seed);
        l.setSampleRate (sr);
        l.setShape (shape);
        l.setFrequency (hz);
        return l;
    }

    double midiHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
} // namespace

TEST_CASE ("LFO rate ranges match the manual", "[lfo]")
{
    CHECK (Lfo::rateToHz (0.0, Lfo::Range::slow) == Catch::Approx (0.064).epsilon (0.01));
    CHECK (Lfo::rateToHz (1.0, Lfo::Range::slow) == Catch::Approx (4.8));
    CHECK (Lfo::rateToHz (0.0, Lfo::Range::fast) == Catch::Approx (1.02).epsilon (0.005));
    CHECK (Lfo::rateToHz (1.0, Lfo::Range::fast) == Catch::Approx (65.4).epsilon (0.001));
    // Both ends of the fast range are Cs: C−4 (MIDI −36) and C2 (MIDI 36); slow starts at C−8 (MIDI −84), 10 octaves below C2.
    CHECK (Lfo::rateToHz (0.0, Lfo::Range::fast) == Catch::Approx (midiHz (-36.0)).epsilon (1e-6));
    CHECK (Lfo::rateToHz (0.0, Lfo::Range::slow) == Catch::Approx (midiHz (-84.0)).epsilon (1e-6));
    CHECK (Lfo::rateToHz (1.0, Lfo::Range::fast) == Catch::Approx (midiHz (36.0)).epsilon (1e-6));
    // Exponential: the knob's midpoint is the geometric mean.
    CHECK (Lfo::rateToHz (0.5, Lfo::Range::fast) == Catch::Approx (std::sqrt (1.02191 * 65.4064)).epsilon (1e-4));
}

TEST_CASE ("LFO shapes: range and direction", "[lfo]")
{
    const auto shape = GENERATE (Shape::volcano, Shape::square, Shape::reverseSaw, Shape::saw, Shape::sine);
    auto l = make (shape, 2.0);
    float lo = 10.0f, hi = -10.0f;
    for (int i = 0; i < (int) sr * 4; ++i)
    {
        const float v = l.process();
        REQUIRE (std::isfinite (v));
        lo = std::min (lo, v);
        hi = std::max (hi, v);
    }
    CHECK (hi <= 1.1f);
    CHECK (lo >= -1.1f);
    CHECK (hi - lo > 0.5f);
}

TEST_CASE ("LFO saw ramps up, reverse saw ramps down", "[lfo]")
{
    auto up = make (Shape::saw, 1.0);
    auto down = make (Shape::reverseSaw, 1.0);
    int rising = 0, falling = 0;
    float prevUp = up.process(), prevDown = down.process();
    for (int i = 0; i < 24000; ++i)
    {
        const float u = up.process(), d = down.process();
        rising += u > prevUp ? 1 : 0;
        falling += d < prevDown ? 1 : 0;
        prevUp = u;
        prevDown = d;
    }
    CHECK (rising > 23000);
    CHECK (falling > 23000);
}

TEST_CASE ("LFO frequency is accurate", "[lfo]")
{
    auto l = make (Shape::sine, 5.0);
    int crossings = 0;
    float prev = l.process();
    for (int i = 0; i < (int) sr * 10; ++i)
    {
        const float v = l.process();
        if (prev < 0.0f && v >= 0.0f)
            ++crossings;
        prev = v;
    }
    CHECK (std::abs (crossings - 50) <= 1);
}

TEST_CASE ("Volcano: two new random values per cycle, slewed (no jumps)", "[lfo]")
{
    auto l = make (Shape::volcano, 10.0);
    float prev = l.process();
    double maxStep = 0.0;
    int turningPoints = 0;
    float prevDelta = 0.0f;
    for (int i = 0; i < (int) sr; ++i) // 10 cycles
    {
        const float v = l.process();
        maxStep = std::max (maxStep, (double) std::abs (v - prev));
        const float delta = v - prev;
        if (delta * prevDelta < 0.0f)
            ++turningPoints;
        if (delta != 0.0f)
            prevDelta = delta;
        prev = v;
    }
    // Slewed: never jumps. A full swing over a half cycle at 10 Hz is ~2 / 2400 per sample, ×π/2 at the steepest.
    CHECK (maxStep < 0.002);
    // At most one turn per half cycle (20 targets in 1 s); random targets usually alternate direction.
    CHECK (turningPoints <= 20);
    CHECK (turningPoints >= 5);
}

TEST_CASE ("Volcano: different seeds give different sequences; same seed repeats", "[lfo]")
{
    auto a = make (Shape::volcano, 5.0, 11), b = make (Shape::volcano, 5.0, 22), c = make (Shape::volcano, 5.0, 11);
    double diffAB = 0.0, diffAC = 0.0;
    for (int i = 0; i < 48000; ++i)
    {
        const float x = a.process(), y = b.process(), z = c.process();
        diffAB += std::abs (x - y);
        diffAC += std::abs (x - z);
    }
    CHECK (diffAB > 100.0);
    CHECK (diffAC == 0.0);
}

TEST_CASE ("LFO retrigger restarts the cycle", "[lfo]")
{
    auto l = make (Shape::saw, 1.0);
    for (int i = 0; i < 20000; ++i)
        l.process();
    l.reset();
    l.process(); // the band-limited reset lands mid-jump
    CHECK (l.process() == Catch::Approx (-1.0f).margin (0.01)); // then the saw starts at the bottom
}

TEST_CASE ("Phase distortion: zero is an exact bypass", "[phase distortion]")
{
    rotor::Oscillator a, b;
    for (auto* o : { &a, &b })
    {
        o->setSampleRate (sr);
        o->setWaveform (rotor::Waveform::saw);
        o->setFrequency (220.0);
    }
    b.setPhaseDistortion (0.0);
    for (int i = 0; i < 4800; ++i)
        REQUIRE (a.process() == b.process());
}

TEST_CASE ("Phase distortion brightens a sine, in both directions", "[phase distortion]")
{
    auto harmonics = [] (double pd)
    {
        rotor::Oscillator o;
        o.setSampleRate (sr);
        o.setWaveform (rotor::Waveform::sine);
        const size_t n = 4096, bin = 64;
        o.setFrequency (bin * sr / n);
        o.setPhaseDistortion (pd);
        std::vector<std::complex<double>> buf (n);
        for (auto& x : buf)
            x = o.process();
        testing::fft (buf);
        double total = 0.0;
        for (size_t k = 1; k < n / 2; ++k)
            total += std::norm (buf[k]);
        return 1.0 - std::norm (buf[bin]) / total;
    };
    CHECK (harmonics (0.0) < 1e-6);
    CHECK (harmonics (0.5) > 0.01);
    CHECK (harmonics (0.9) > harmonics (0.5));
    CHECK (harmonics (-0.5) == Catch::Approx (harmonics (0.5)).epsilon (0.05)); // mirror image
}

TEST_CASE ("Phase distortion keeps the pitch", "[phase distortion]")
{
    const double pd = GENERATE (-0.9, -0.4, 0.4, 0.9);
    rotor::Oscillator o;
    o.setSampleRate (sr);
    o.setWaveform (rotor::Waveform::sine);
    o.setFrequency (440.0);
    o.setPhaseDistortion (pd);
    int crossings = 0;
    float prev = o.process();
    for (int i = 0; i < (int) sr; ++i)
    {
        const float v = o.process();
        if (prev < 0.0f && v >= 0.0f)
            ++crossings;
        prev = v;
    }
    CHECK (std::abs (crossings - 440) <= 1);
}
