#include "dsp/Envelope.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using rotor::Envelope;

namespace
{
    constexpr double sr = 48000.0;

    Envelope makeEnvelope (double a, double d, double s, double r)
    {
        Envelope e;
        e.setSampleRate (sr);
        e.setParameters ({ a, d, s, r });
        return e;
    }

    void run (Envelope& e, int samples)
    {
        for (int i = 0; i < samples; ++i)
            e.process();
    }
} // namespace

TEST_CASE ("Envelope is idle and silent until triggered", "[envelope]")
{
    auto e = makeEnvelope (0.01, 0.1, 0.5, 0.1);
    CHECK_FALSE (e.isActive());
    CHECK (e.process() == 0.0f);
}

TEST_CASE ("Envelope runs attack, decay, sustain, release in order", "[envelope]")
{
    auto e = makeEnvelope (0.01, 0.1, 0.5, 0.1);
    e.noteOn();
    CHECK (e.getStage() == Envelope::Stage::attack);

    // Attack is linear: halfway through, level ≈ 0.5.
    run (e, 240);
    CHECK (e.getLevel() == Catch::Approx (0.5).margin (0.01));

    run (e, 241);
    CHECK (e.getStage() == Envelope::Stage::decay);

    run (e, (int) (0.1 * sr) + 10);
    CHECK (e.getStage() == Envelope::Stage::sustain);
    CHECK (e.getLevel() == Catch::Approx (0.5));

    e.noteOff();
    CHECK (e.getStage() == Envelope::Stage::release);
    run (e, (int) (0.1 * sr) + 10);
    CHECK_FALSE (e.isActive());
    CHECK (e.getLevel() == 0.0);
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
    auto e = makeEnvelope (0.01, 0.1, 0.5, 1.0);
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

TEST_CASE ("Zero sustain decays to silence but stays in sustain stage", "[envelope]")
{
    auto e = makeEnvelope (0.001, 0.05, 0.0, 0.05);
    e.noteOn();
    run (e, 48000);
    CHECK (e.getStage() == Envelope::Stage::sustain);
    CHECK (e.getLevel() == 0.0);
}
