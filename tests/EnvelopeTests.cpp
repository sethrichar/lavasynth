#include "dsp/ControlFold.h"
#include "dsp/Envelope.h"
#include "dsp/HostSync.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using rotor::Envelope;

namespace
{
    constexpr double sr = 48000.0;

    Envelope makeEnvelope (double a, double d, double s, double r, bool loop = false)
    {
        Envelope e;
        e.setSampleRate (sr);
        e.setParameters ({ a, d, s, r, loop });
        return e;
    }

    void run (Envelope& e, int samples)
    {
        for (int i = 0; i < samples; ++i)
            e.process();
    }

    // Samples until the envelope leaves `stage`.
    int samplesIn (Envelope& e, Envelope::Stage stage)
    {
        int n = 0;
        while (e.getStage() == stage && n < 100'000'000)
        {
            e.process();
            ++n;
        }
        return n;
    }

    // Loop frequency from the spacing of attack starts.
    double loopHz (Envelope& e)
    {
        run (e, (int) sr); // settle into the loop
        int starts = 0, first = -1, last = -1;
        auto prev = e.getStage();
        for (int i = 0; i < (int) sr * 2; ++i)
        {
            e.process();
            const auto now = e.getStage();
            if (now == Envelope::Stage::attack && prev != Envelope::Stage::attack)
            {
                if (first < 0) first = i;
                last = i;
                ++starts;
            }
            prev = now;
        }
        return starts > 1 ? (starts - 1) * sr / (last - first) : 0.0;
    }
} // namespace

TEST_CASE ("Envelope is idle and silent until triggered", "[envelope]")
{
    auto e = makeEnvelope (0.05, 0.1, 0.5, 0.1);
    CHECK_FALSE (e.isActive());
    CHECK (e.process() == 0.0f);
}

TEST_CASE ("Stage times are full-swing times", "[envelope]")
{
    auto e = makeEnvelope (0.1, 0.2, 0.0, 0.3);
    e.noteOn();
    CHECK (samplesIn (e, Envelope::Stage::attack) == Catch::Approx (0.1 * sr).margin (2));
    CHECK (samplesIn (e, Envelope::Stage::decay) == Catch::Approx (0.2 * sr).margin (2));
    CHECK (e.getStage() == Envelope::Stage::sustain);
    CHECK (e.getLevel() == 0.0);
}

TEST_CASE ("Envelope runs attack, decay, sustain, release in order", "[envelope]")
{
    auto e = makeEnvelope (0.05, 0.1, 0.5, 0.1);
    e.noteOn();
    CHECK (e.getStage() == Envelope::Stage::attack);
    samplesIn (e, Envelope::Stage::attack);
    CHECK (e.getStage() == Envelope::Stage::decay);
    samplesIn (e, Envelope::Stage::decay);
    CHECK (e.getStage() == Envelope::Stage::sustain);
    CHECK (e.getLevel() == Catch::Approx (0.5));

    e.noteOff();
    CHECK (e.getStage() == Envelope::Stage::release);
    samplesIn (e, Envelope::Stage::release);
    CHECK_FALSE (e.isActive());
    CHECK (e.getLevel() == 0.0);
}

TEST_CASE ("Attack curve is analog-style (fast start, slowing near the top)", "[envelope]")
{
    auto e = makeEnvelope (0.1, 0.1, 0.5, 0.1);
    e.noteOn();
    run (e, (int) (0.05 * sr));
    CHECK (e.getLevel() > 0.6); // RC charge toward 1.2: ~0.71 at half time
    CHECK (e.getLevel() < 0.8);
}

TEST_CASE ("Higher sustain shortens the decay", "[envelope]")
{
    auto low = makeEnvelope (0.02, 0.5, 0.1, 0.1);
    auto high = makeEnvelope (0.02, 0.5, 0.8, 0.1);
    low.noteOn();
    high.noteOn();
    samplesIn (low, Envelope::Stage::attack);
    samplesIn (high, Envelope::Stage::attack);
    CHECK (samplesIn (high, Envelope::Stage::decay) < samplesIn (low, Envelope::Stage::decay) / 4);
}

TEST_CASE ("Releasing during attack goes straight to release", "[envelope]")
{
    auto e = makeEnvelope (1.0, 0.1, 0.5, 0.05);
    e.noteOn();
    run (e, 100);
    e.noteOff();
    CHECK (e.getStage() == Envelope::Stage::release);
    const double before = e.getLevel();
    e.process();
    CHECK (e.getLevel() < before);
}

TEST_CASE ("Retrigger continues from the current level (no click)", "[envelope]")
{
    auto e = makeEnvelope (0.02, 0.1, 0.5, 1.0);
    e.noteOn();
    run (e, 48000);
    e.noteOff();
    run (e, 100);
    const double level = e.getLevel();
    REQUIRE (level > 0.3);
    e.noteOn();
    e.process();
    CHECK (e.getLevel() >= level);
    CHECK (e.getLevel() - level < 0.01);
}

TEST_CASE ("Stage times never go below the minimum", "[envelope]")
{
    auto e = makeEnvelope (0.0, 0.0, 0.0, 0.0);
    e.noteOn();
    CHECK (samplesIn (e, Envelope::Stage::attack) == Catch::Approx (Envelope::minTimeSeconds * sr).margin (2));
}

TEST_CASE ("Loop: cycles attack → decay while held", "[envelope][loop]")
{
    auto e = makeEnvelope (0.05, 0.05, 0.0, 0.1, true);
    e.noteOn();
    samplesIn (e, Envelope::Stage::attack);
    samplesIn (e, Envelope::Stage::decay);
    CHECK (e.getStage() == Envelope::Stage::attack);
    CHECK (loopHz (e) == Catch::Approx (10.0).epsilon (0.01)); // 50 + 50 ms
}

TEST_CASE ("Loop: fastest loop (A, D, S at minimum) is C1, 32.7 Hz", "[envelope][loop]")
{
    auto e = makeEnvelope (0.0, 0.0, 0.0, 0.1, true);
    e.noteOn();
    CHECK (loopHz (e) == Catch::Approx (32.703).epsilon (0.005));
}

TEST_CASE ("Loop: raising sustain speeds up the loop and narrows its depth", "[envelope][loop]")
{
    auto low = makeEnvelope (0.05, 0.05, 0.0, 0.1, true);
    auto high = makeEnvelope (0.05, 0.05, 0.6, 0.1, true);
    low.noteOn();
    high.noteOn();
    CHECK (loopHz (high) > 2.0 * loopHz (low));

    double lowest = 1.0;
    for (int i = 0; i < 48000; ++i)
        lowest = std::min (lowest, (double) high.process());
    CHECK (lowest == Catch::Approx (0.6).margin (0.001));
}

TEST_CASE ("Loop: releasing mid-cycle goes to release", "[envelope][loop]")
{
    auto e = makeEnvelope (0.05, 0.05, 0.0, 0.1, true);
    e.noteOn();
    run (e, 3000);
    e.noteOff();
    CHECK (e.getStage() == Envelope::Stage::release);
    samplesIn (e, Envelope::Stage::release);
    CHECK_FALSE (e.isActive());
}

TEST_CASE ("Loop at full sustain just holds", "[envelope][loop]")
{
    auto e = makeEnvelope (0.02, 0.02, 1.0, 0.1, true);
    e.noteOn();
    run (e, 48000);
    CHECK (e.getStage() == Envelope::Stage::sustain);
    CHECK (e.getLevel() == 1.0);
}

TEST_CASE ("Key tracking scales envelope speed per octave", "[envelope]")
{
    using rotor::keyTrackedRate;
    CHECK (keyTrackedRate (60.0, 1.0) == Catch::Approx (1.0));
    CHECK (keyTrackedRate (72.0, 1.0) == Catch::Approx (2.0));  // clockwise: higher = faster
    CHECK (keyTrackedRate (72.0, -1.0) == Catch::Approx (0.5)); // counter-clockwise: higher = slower
    CHECK (keyTrackedRate (48.0, -1.0) == Catch::Approx (2.0)); //   …so lower = faster
    CHECK (keyTrackedRate (84.0, 0.0) == Catch::Approx (1.0));

    // With full tracking, the looping envelope's rate follows pitch.
    auto e = makeEnvelope (0.0, 0.0, 0.0, 0.1, true);
    e.setRateScale (keyTrackedRate (72.0, 1.0));
    e.noteOn();
    CHECK (loopHz (e) == Catch::Approx (2.0 * 32.703).epsilon (0.01));
}

TEST_CASE ("foldIntoRange reflects instead of clipping", "[modulation]")
{
    using rotor::foldIntoRange;
    CHECK (foldIntoRange (0.3, 0.0, 1.0) == Catch::Approx (0.3));
    CHECK (foldIntoRange (1.2, 0.0, 1.0) == Catch::Approx (0.8));
    CHECK (foldIntoRange (-0.25, 0.0, 1.0) == Catch::Approx (0.25));
    CHECK (foldIntoRange (2.3, 0.0, 1.0) == Catch::Approx (0.3));   // folded twice
    CHECK (foldIntoRange (-1.7, 0.0, 1.0) == Catch::Approx (0.3));
    CHECK (foldIntoRange (1.5, -1.0, 1.0) == Catch::Approx (0.5));  // bipolar range
    CHECK (foldIntoRange (1.0, 0.0, 1.0) == Catch::Approx (1.0));
    for (double x = -5.0; x <= 5.0; x += 0.01)
    {
        const double y = foldIntoRange (x, -1.0, 1.0);
        REQUIRE (y >= -1.0);
        REQUIRE (y <= 1.0);
    }
}

TEST_CASE ("Host sync snaps times to note values", "[sync]")
{
    using rotor::NoteValues;
    // 120 BPM: quarter = 0.5 s, eighth = 0.25 s, dotted quarter = 0.75 s, quarter triplet = 1/3 s.
    CHECK (NoteValues::snapSeconds (0.48, 120.0) == Catch::Approx (0.5));
    CHECK (NoteValues::snapSeconds (0.26, 120.0) == Catch::Approx (0.25));
    CHECK (NoteValues::snapSeconds (0.72, 120.0) == Catch::Approx (0.75));
    CHECK (NoteValues::snapSeconds (0.34, 120.0) == Catch::Approx (1.0 / 3.0));
    // Tempo change moves the grid: 60 BPM quarter = 1 s.
    CHECK (NoteValues::snapSeconds (0.95, 60.0) == Catch::Approx (1.0));
    // Longer than the longest note value → the longest (16 bars at 120 = 48 s dotted).
    CHECK (NoteValues::snapSeconds (3600.0, 120.0) == Catch::Approx (48.0));
}

TEST_CASE ("Every envelope curve keeps its stage times and lands on its levels", "[envelope][voicing]")
{
    const auto curve = GENERATE (Envelope::Curve::rotor, Envelope::Curve::punchy, Envelope::Curve::vintagePoly,
                                 Envelope::Curve::snappyDigital, Envelope::Curve::linear);
    Envelope e;
    e.setSampleRate (sr);
    e.setParameters ({ 0.1, 0.2, 0.3, 0.3, false, curve });
    e.noteOn();
    CHECK (samplesIn (e, Envelope::Stage::attack) == Catch::Approx (0.1 * sr).margin (2));
    samplesIn (e, Envelope::Stage::decay);
    CHECK (e.getLevel() == Catch::Approx (0.3));
    e.noteOff();
    samplesIn (e, Envelope::Stage::release);
    CHECK (e.getLevel() == 0.0);

    // Loop tuning holds for every curve (full swing A + D at minimum = C1).
    Envelope l;
    l.setSampleRate (sr);
    l.setParameters ({ 0.0, 0.0, 0.0, 0.1, true, curve });
    l.noteOn();
    CHECK (loopHz (l) == Catch::Approx (32.703).epsilon (0.005));
}

TEST_CASE ("Envelope curves differ in shape: linear is straight, snappy drops fastest", "[envelope][voicing]")
{
    auto levelAfterDecayFraction = [] (Envelope::Curve curve, double fraction)
    {
        Envelope e;
        e.setSampleRate (sr);
        e.setParameters ({ 0.0, 1.0, 0.0, 0.3, false, curve });
        e.noteOn();
        samplesIn (e, Envelope::Stage::attack);
        run (e, (int) (fraction * sr));
        return e.getLevel();
    };
    CHECK (levelAfterDecayFraction (Envelope::Curve::linear, 0.5) == Catch::Approx (0.5).margin (0.01));
    const double snappy = levelAfterDecayFraction (Envelope::Curve::snappyDigital, 0.1);
    const double rotor = levelAfterDecayFraction (Envelope::Curve::rotor, 0.1);
    const double vintage = levelAfterDecayFraction (Envelope::Curve::vintagePoly, 0.1);
    CHECK (snappy < rotor);
    CHECK (rotor < vintage);
}
