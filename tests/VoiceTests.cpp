#include "dsp/Voice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using rotor::Voice;

TEST_CASE ("MIDI note to frequency", "[voice]")
{
    CHECK (Voice::midiNoteToHz (69) == Catch::Approx (440.0));
    CHECK (Voice::midiNoteToHz (24) == Catch::Approx (32.7032).epsilon (1e-4)); // C1
}

TEST_CASE ("Voice is silent until a note, then sounds, then decays to silence", "[voice]")
{
    Voice v;
    v.prepare (48000.0);
    Voice::Parameters p;
    p.amp = { 0.001, 0.05, 0.8, 0.02 };
    v.setParameters (p);

    for (int i = 0; i < 100; ++i)
        REQUIRE (v.process() == 0.0f);

    v.noteOn (60, 1.0f);
    double energy = 0.0;
    for (int i = 0; i < 4800; ++i)
    {
        const float s = v.process();
        REQUIRE (std::isfinite (s));
        energy += s * s;
    }
    CHECK (energy > 10.0);

    v.noteOff();
    for (int i = 0; i < 48000; ++i)
        v.process();
    CHECK_FALSE (v.isActive());
    CHECK (v.process() == 0.0f);
}

TEST_CASE ("Every waveform and octave produces finite output", "[voice]")
{
    Voice v;
    v.prepare (44100.0);
    for (int w = 0; w < rotor::numWaveforms; ++w)
    {
        for (int oct = -2; oct <= 2; ++oct)
        {
            Voice::Parameters p;
            p.waveform = static_cast<rotor::Waveform> (w);
            p.octave = oct;
            p.resonance = 1.0;
            v.setParameters (p);
            v.noteOn (127, 1.0f);
            for (int i = 0; i < 2000; ++i)
                REQUIRE (std::isfinite (v.process()));
            v.noteOn (0, 1.0f);
            for (int i = 0; i < 2000; ++i)
                REQUIRE (std::isfinite (v.process()));
        }
    }
}
