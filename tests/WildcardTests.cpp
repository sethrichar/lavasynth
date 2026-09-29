#include "dsp/Voice.h"
#include "dsp/Wildcards.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <set>

using rotor::Wildcards;
namespace wc = rotor::wildcard;

namespace
{
    constexpr double dt = 1.0 / 24000.0; // the voice's modulation rate at 48 kHz

    Wildcards make (Wildcards::Amounts a, std::uint32_t seed = 7)
    {
        Wildcards w (seed);
        w.setAmounts (a);
        return w;
    }

    // Counts direction changes of a signal over `seconds` — a rough speed measure.
    template <typename Fn>
    int turns (Wildcards& w, double seconds, Fn read)
    {
        int count = 0;
        double prev = read (w), prevDelta = 0.0;
        for (int i = 0; i < (int) (seconds / dt); ++i)
        {
            w.tick (dt);
            const double v = read (w), d = v - prev;
            if (d * prevDelta < 0.0) ++count;
            if (d != 0.0) prevDelta = d;
            prev = v;
        }
        return count;
    }
} // namespace

TEST_CASE ("Wildcards at zero are exactly neutral", "[wildcards]")
{
    auto w = make ({});
    for (int i = 0; i < 48000; ++i)
    {
        if (i % 1000 == 0)
            w.trigger();
        w.tick (dt);
        REQUIRE (w.getPitchCents() == 0.0);
        REQUIRE (w.getGain() == 1.0);
        REQUIRE (w.getCutoffOctaves() == 0.0);
        REQUIRE (w.getNoiseBurstLevel() == 0.0);
    }
    const auto s = w.drawScatter (false, false, false);
    CHECK (s.attack == 1.0);
    CHECK (s.decay == 1.0);
    CHECK (s.release == 1.0);
}

TEST_CASE ("Note detune: new random offset on each key press, held between presses", "[wildcards]")
{
    Wildcards::Amounts a;
    a.noteDetune = 1.0;
    auto w = make (a);
    std::set<long> seen;
    for (int n = 0; n < 50; ++n)
    {
        w.trigger();
        w.tick (dt);
        const double cents = w.getPitchCents();
        REQUIRE (std::abs (cents) <= wc::noteDetuneMaxCents);
        for (int i = 0; i < 100; ++i)
        {
            w.tick (dt);
            REQUIRE (w.getPitchCents() == cents); // constant until the next press
        }
        seen.insert (std::lround (cents * 100.0));
    }
    CHECK (seen.size() > 40);

    // Half amount = half the range.
    a.noteDetune = 0.5;
    auto half = make (a);
    for (int n = 0; n < 50; ++n)
    {
        half.trigger();
        half.tick (dt);
        REQUIRE (std::abs (half.getPitchCents()) <= 0.5 * wc::noteDetuneMaxCents);
    }
}

TEST_CASE ("Wow is slow, flutter is fast, both bounded", "[wildcards]")
{
    Wildcards::Amounts wowOnly, flutterOnly;
    wowOnly.wow = 1.0;
    flutterOnly.flutter = 1.0;
    auto w = make (wowOnly), f = make (flutterOnly);

    const auto cents = [] (Wildcards& x) { return x.getPitchCents(); };
    const int wowTurns = turns (w, 20.0, cents);
    const int flutterTurns = turns (f, 20.0, cents);
    CHECK (flutterTurns > 5 * wowTurns);
    CHECK (wowTurns > 3);

    for (int i = 0; i < 480000; ++i)
    {
        w.tick (dt);
        f.tick (dt);
        REQUIRE (std::abs (w.getPitchCents()) <= wc::wowMaxCents);
        REQUIRE (std::abs (f.getPitchCents()) <= wc::flutterMaxCents);
    }
}

TEST_CASE ("Reel drag: occasional downward bursts that recover", "[wildcards]")
{
    Wildcards::Amounts a;
    a.reelDrag = 1.0;
    auto w = make (a);
    int bursts = 0;
    bool inBurst = false;
    double deepest = 0.0;
    for (int i = 0; i < (int) (60.0 / dt); ++i) // one minute
    {
        w.tick (dt);
        const double c = w.getPitchCents();
        REQUIRE (c <= 0.0); // only ever drags down
        REQUIRE (c >= -wc::reelDragMaxCents);
        deepest = std::min (deepest, c);
        if (! inBurst && c < -1.0) { inBurst = true; ++bursts; }
        if (inBurst && c == 0.0) inBurst = false;
    }
    // ~0.8 per second expected; allow wide randomness.
    CHECK (bursts > 20);
    CHECK (bursts < 90);
    CHECK (deepest < -0.3 * wc::reelDragMaxCents);
}

TEST_CASE ("Chaos: volume only dips, cutoff wanders both ways, noise bursts happen", "[wildcards]")
{
    Wildcards::Amounts a;
    a.chaos = 1.0;
    auto w = make (a);
    double minGain = 1.0, lo = 0.0, hi = 0.0;
    int burstTicks = 0;
    for (int i = 0; i < (int) (20.0 / dt); ++i)
    {
        w.tick (dt);
        REQUIRE (w.getGain() <= 1.0);
        minGain = std::min (minGain, w.getGain());
        lo = std::min (lo, w.getCutoffOctaves());
        hi = std::max (hi, w.getCutoffOctaves());
        burstTicks += w.getNoiseBurstLevel() > 0.0 ? 1 : 0;
    }
    CHECK (20.0 * std::log10 (minGain) < -0.5 * wc::chaosMaxDipDb);
    CHECK (20.0 * std::log10 (minGain) >= -wc::chaosMaxDipDb - 1e-9);
    CHECK (lo < -0.5 * wc::chaosCutoffOctaves);
    CHECK (hi > 0.5 * wc::chaosCutoffOctaves);
    // 3 bursts/s × ~35 ms average ≈ 10% of the time.
    const double share = burstTicks * dt / 20.0;
    CHECK (share > 0.03);
    CHECK (share < 0.25);
}

TEST_CASE ("Envelope scatter follows the manual's rules", "[wildcards][scatter]")
{
    Wildcards::Amounts a;
    a.envScatter = 1.0;
    auto w = make (a);
    const double maxFactor = std::exp2 (wc::envScatterMaxOctaves);

    for (int i = 0; i < 200; ++i)
    {
        // attack 0, decay > 0 → decay only
        auto s = w.drawScatter (true, false, false);
        REQUIRE (s.attack == 1.0);
        REQUIRE (s.decay != 1.0);

        // attack > 0, decay 0 → attack only
        s = w.drawScatter (false, true, false);
        REQUIRE (s.attack != 1.0);
        REQUIRE (s.decay == 1.0);

        // both 0 → both, and a stage at its minimum can only get longer
        s = w.drawScatter (true, true, false);
        REQUIRE (s.attack >= 1.0);
        REQUIRE (s.decay >= 1.0);

        // release at maximum → left alone
        s = w.drawScatter (false, false, true);
        REQUIRE (s.release == 1.0);

        // everything within ×/÷ 2^max
        s = w.drawScatter (false, false, false);
        for (double f : { s.attack, s.decay, s.release })
        {
            REQUIRE (f <= maxFactor + 1e-9);
            REQUIRE (f >= 1.0 / maxFactor - 1e-9);
        }
    }
}

TEST_CASE ("Stereo spreader: static positions scale with the slider", "[wildcards][spread]")
{
    using rotor::spreadPan;
    for (int v = 0; v < 5; ++v)
        CHECK (spreadPan (v, 0.0, 0.0, 0.0) == 0.0); // no spread: all centred

    CHECK (spreadPan (0, 1.0, 0.0, 0.0) == 0.0);    // voice 1 centre
    CHECK (spreadPan (1, 1.0, 0.0, 0.0) < 0.0);     // 2 and 4 left
    CHECK (spreadPan (3, 1.0, 0.0, 0.0) < 0.0);
    CHECK (spreadPan (2, 1.0, 0.0, 0.0) > 0.0);     // 3 and 5 right
    CHECK (spreadPan (4, 1.0, 0.0, 0.0) > 0.0);
    CHECK (spreadPan (1, 0.5, 0.0, 0.0) == Catch::Approx (0.5 * spreadPan (1, 1.0, 0.0, 0.0)));
}

TEST_CASE ("Stereo spreader: negative mod env reverses the order and moves voice 1", "[wildcards][spread]")
{
    using rotor::spreadPan;
    // Slider at 0, envelope pushing −0.8 → voices swap sides.
    CHECK (spreadPan (1, 0.0, -0.8, 0.0) > 0.0);
    CHECK (spreadPan (2, 0.0, -0.8, 0.0) < 0.0);
    CHECK (spreadPan (0, 0.0, -0.8, 0.0) != 0.0); // voice 1 moves too
    CHECK (spreadPan (0, 0.0, 0.4, 0.0) == Catch::Approx (0.4));
}

TEST_CASE ("Stereo spreader: LFO moves each voice on its own; everything folds at the edges", "[wildcards][spread]")
{
    using rotor::spreadPan;
    CHECK (spreadPan (0, 0.0, 0.0, 0.3) == Catch::Approx (0.3));
    CHECK (spreadPan (2, 1.0, 0.0, 0.5) == Catch::Approx (0.5)); // +1 + 0.5 → folds back to 0.5
    CHECK (spreadPan (1, 1.0, 0.8, 0.0) == Catch::Approx (-0.2));  // width 1.8 → folds to 0.2, voice 2 at −0.2
    for (double s = 0.0; s <= 1.0; s += 0.1)
        for (double e = -2.0; e <= 2.0; e += 0.1)
            for (int v = 0; v < 5; ++v)
            {
                const double p = spreadPan (v, s, e, 0.7);
                REQUIRE (p >= -1.0);
                REQUIRE (p <= 1.0);
            }
}

TEST_CASE ("Constant-power pan law", "[wildcards][spread]")
{
    float l, r;
    rotor::panGains (0.0, l, r);
    CHECK (l == Catch::Approx (std::sqrt (0.5)));
    CHECK (r == Catch::Approx (std::sqrt (0.5)));
    rotor::panGains (-1.0, l, r);
    CHECK (l == Catch::Approx (1.0));
    CHECK (r == Catch::Approx (0.0).margin (1e-6));
    for (double p = -1.0; p <= 1.0; p += 0.05)
    {
        rotor::panGains (p, l, r);
        REQUIRE (l * l + r * r == Catch::Approx (1.0));
    }
}

TEST_CASE ("Voice applies note detune and envelope scatter per trigger", "[wildcards][voice]")
{
    rotor::Voice v;
    v.prepare (192000.0);
    v.setIdentity (2, 1234);
    rotor::Voice::Parameters p;
    p.wild.noteDetune = 1.0;
    p.wild.envScatter = 1.0;
    p.amp = { 0.2, 0.3, 0.5, 0.4, false };
    v.setParameters (p);

    v.noteOn (60, 1.0f);
    v.process();
    const double first = v.getPitchOffsetCents();
    const auto s1 = v.getScatter();
    v.noteOn (60, 1.0f);
    v.process();
    CHECK (v.getPitchOffsetCents() != first);
    CHECK (v.getScatter().attack != s1.attack);
    CHECK (std::abs (v.getPitchOffsetCents()) <= rotor::wildcard::noteDetuneMaxCents);
}

TEST_CASE ("Voice pans by its spreader position", "[wildcards][voice]")
{
    const int voice = GENERATE (0, 1, 2, 3, 4);
    rotor::Voice v;
    v.prepare (192000.0);
    v.setIdentity (voice, 99);
    rotor::Voice::Parameters p;
    p.spread = 1.0;
    v.setParameters (p);
    v.noteOn (60, 1.0f);
    v.process();
    CHECK (v.getPan() == Catch::Approx (rotor::wildcard::spreadPositions[voice]));
}
