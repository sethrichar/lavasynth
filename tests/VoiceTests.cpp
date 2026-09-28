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

TEST_CASE ("Voice level scales the output; level 0 is silent", "[voice]")
{
    auto energyAt = [] (double level)
    {
        Voice v;
        v.prepare (48000.0);
        Voice::Parameters p;
        p.level = level;
        p.amp = { 0.001, 0.05, 1.0, 0.02 };
        v.setParameters (p);
        v.reset(); // start the level smoother at the set level
        v.noteOn (60, 1.0f);
        double e = 0.0;
        for (int i = 0; i < 4800; ++i)
        {
            const float s = v.process();
            e += s * s;
        }
        return e;
    };
    CHECK (energyAt (0.0) == 0.0);
    CHECK (energyAt (0.5) == Catch::Approx (0.25 * energyAt (1.0)).epsilon (0.01));
}

TEST_CASE ("Voice glides from its previous note", "[voice]")
{
    Voice v;
    v.prepare (48000.0);
    Voice::Parameters p;
    p.glideSecondsPerOctave = 0.1;
    p.octave = 1;
    v.setParameters (p);

    v.noteOn (60, 1.0f);
    CHECK (v.getCurrentPitch() == 72.0); // first note: no glide, octave +1 applied
    v.noteOn (72, 1.0f);
    v.process();
    CHECK (v.getCurrentPitch() > 72.0);
    CHECK (v.getCurrentPitch() < 84.0);
    for (int i = 0; i < 48000; ++i)
        v.process();
    CHECK (v.getCurrentPitch() == 84.0);
}

TEST_CASE ("Sub oscillator plays one or two octaves down and follows the voice", "[voice]")
{
    // Main osc off, sub only, filter wide open: count the sub's zero crossings.
    auto subHz = [] (int subOctave, int voiceOctave)
    {
        Voice v;
        v.prepare (48000.0);
        Voice::Parameters p;
        p.oscLevel = 0.0;
        p.subLevel = 1.0;
        p.subOctave = subOctave;
        p.octave = voiceOctave;
        p.cutoffHz = 20000.0;
        p.resonance = 0.0;
        p.amp = { 0.001, 0.1, 1.0, 0.1 };
        v.setParameters (p);
        v.noteOn (69, 1.0f); // A4 = 440 Hz
        for (int i = 0; i < 4800; ++i)
            v.process();
        int crossings = 0;
        float prev = v.process();
        for (int i = 0; i < 48000; ++i)
        {
            const float s = v.process();
            if (prev < 0.0f && s >= 0.0f)
                ++crossings;
            prev = s;
        }
        return crossings;
    };
    CHECK (std::abs (subHz (1, 0) - 220) <= 2);
    CHECK (std::abs (subHz (2, 0) - 110) <= 2);
    CHECK (std::abs (subHz (1, 1) - 440) <= 2);
}

TEST_CASE ("Noise input only sounds when the noise level is up", "[voice]")
{
    auto energy = [] (double noiseLevel)
    {
        Voice v;
        v.prepare (48000.0);
        Voice::Parameters p;
        p.oscLevel = 0.0;
        p.noiseLevel = noiseLevel;
        v.setParameters (p);
        v.noteOn (60, 1.0f);
        double e = 0.0;
        for (int i = 0; i < 4800; ++i)
        {
            const float s = v.process ((i % 7) < 3 ? 0.5f : -0.4f);
            e += s * s;
        }
        return e;
    };
    CHECK (energy (0.0) == 0.0);
    CHECK (energy (1.0) > 1.0);
}

TEST_CASE ("Filter key tracking follows the played pitch", "[voice]")
{
    Voice v;
    v.prepare (192000.0);
    Voice::Parameters p;
    p.cutoffHz = 1000.0;
    p.keyTrack = 1.0;
    v.setParameters (p);
    v.noteOn (72, 1.0f);
    CHECK (v.getFilterCutoff() == Catch::Approx (2000.0));
    v.moveTo (48);
    CHECK (v.getFilterCutoff() == Catch::Approx (500.0));
}
