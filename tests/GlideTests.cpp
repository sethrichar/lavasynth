#include "dsp/Glide.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using rotor::Glide;

namespace
{
    constexpr double sr = 48000.0;

    int samplesToArrive (Glide& g)
    {
        int n = 0;
        while (g.isGliding() && n < 10'000'000)
        {
            g.process();
            ++n;
        }
        return n;
    }
} // namespace

TEST_CASE ("Glide off jumps straight to the note", "[glide]")
{
    Glide g;
    g.setSampleRate (sr);
    g.setTimePerOctave (0.0);
    g.setTarget (60);
    g.setTarget (72);
    CHECK_FALSE (g.isGliding());
    CHECK (g.getCurrent() == 72.0);
}

TEST_CASE ("First note never glides", "[glide]")
{
    Glide g;
    g.setSampleRate (sr);
    g.setTimePerOctave (1.0);
    g.setTarget (60);
    CHECK_FALSE (g.isGliding());
    CHECK (g.getCurrent() == 60.0);
}

TEST_CASE ("Glide time scales with interval size", "[glide]")
{
    Glide g;
    g.setSampleRate (sr);
    g.setTimePerOctave (0.5);

    g.jumpTo (60);
    g.setTarget (61);
    const int halfStep = samplesToArrive (g);

    g.jumpTo (60);
    g.setTarget (84);
    const int twoOctaves = samplesToArrive (g);

    CHECK (twoOctaves == Catch::Approx (24.0 * halfStep).epsilon (0.01));
    CHECK (twoOctaves == Catch::Approx (sr).epsilon (0.01)); // 2 octaves at 0.5 s/oct = 1 s
}

TEST_CASE ("Descending glides take ~10% longer than ascending", "[glide]")
{
    Glide g;
    g.setSampleRate (sr);
    g.setTimePerOctave (0.25);

    g.jumpTo (48);
    g.setTarget (60);
    const int up = samplesToArrive (g);

    g.setTarget (48);
    const int down = samplesToArrive (g);

    CHECK ((double) down / up == Catch::Approx (1.1).epsilon (0.01));
}

TEST_CASE ("Glide moves monotonically and lands exactly on the target", "[glide]")
{
    Glide g;
    g.setSampleRate (sr);
    g.setTimePerOctave (0.1);
    g.jumpTo (72);
    g.setTarget (65);
    double prev = g.getCurrent();
    while (g.isGliding())
    {
        const double x = g.process();
        REQUIRE (x <= prev);
        REQUIRE (x >= 65.0);
        prev = x;
    }
    CHECK (g.getCurrent() == 65.0);
}

TEST_CASE ("Turning glide off mid-glide snaps to the target", "[glide]")
{
    Glide g;
    g.setSampleRate (sr);
    g.setTimePerOctave (1.0);
    g.jumpTo (60);
    g.setTarget (72);
    g.process();
    g.setTimePerOctave (0.0);
    CHECK_FALSE (g.isGliding());
    CHECK (g.getCurrent() == 72.0);
}
