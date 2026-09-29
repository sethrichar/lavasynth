#include "dsp/Expression.h"
#include "dsp/Voice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using rotor::Expression;
using rotor::MidiExpression;
namespace perf = rotor::performance;

TEST_CASE ("Normal MIDI: bend, pressure and timbre apply to every note, latest wins", "[expression]")
{
    MidiExpression m;
    m.pitchBend (3, 16383);
    CHECK (m.forChannel (1).bendSemitones == Catch::Approx (2.0).margin (0.001));
    CHECK (m.forChannel (9).bendSemitones == Catch::Approx (2.0).margin (0.001));
    m.pitchBend (5, 8192); // centred on another channel → back to zero for everyone
    CHECK (m.forChannel (3).bendSemitones == 0.0);
    m.pitchBend (1, 0);
    CHECK (m.forChannel (1).bendSemitones == Catch::Approx (-2.0));

    m.channelPressure (2, 127);
    CHECK (m.forChannel (7).pressure == Catch::Approx (1.0));
    m.controller (4, 74, 127);
    CHECK (m.forChannel (1).timbre == Catch::Approx (1.0));
    m.controller (1, 1, 64);
    CHECK (m.getWheel() == Catch::Approx (64.0 / 127.0));
}

TEST_CASE ("MPE: per-note bend, pressure and timbre on member channels", "[expression][mpe]")
{
    MidiExpression m;
    m.setMpe (true);
    m.noteOn (2);
    m.noteOn (3);
    m.pitchBend (2, 16383);        // +48 semitones on channel 2 only
    m.channelPressure (3, 127);
    m.controller (3, 74, 0);
    CHECK (m.forChannel (2).bendSemitones == Catch::Approx (48.0).margin (0.01));
    CHECK (m.forChannel (3).bendSemitones == 0.0);
    CHECK (m.forChannel (2).pressure == 0.0);
    CHECK (m.forChannel (3).pressure == Catch::Approx (1.0));
    CHECK (m.forChannel (3).timbre == 0.0);
    CHECK (m.forChannel (2).timbre == 0.5);

    // The manager channel's bend adds to every note (±2).
    m.pitchBend (1, 0);
    CHECK (m.forChannel (2).bendSemitones == Catch::Approx (46.0).margin (0.01));
    CHECK (m.forChannel (3).bendSemitones == Catch::Approx (-2.0));

    // A new note on a channel starts neutral.
    m.noteOn (3);
    CHECK (m.forChannel (3).pressure == 0.0);
    CHECK (m.forChannel (3).timbre == 0.5);
}

TEST_CASE ("External input routing", "[expression][ext]")
{
    using rotor::ExtSource;
    for (int v = 0; v < 5; ++v)
        CHECK (rotor::extSourceFor (v, false) == ExtSource::mono);
    CHECK (rotor::extSourceFor (0, true) == ExtSource::left);
    CHECK (rotor::extSourceFor (1, true) == ExtSource::right);
    CHECK (rotor::extSourceFor (2, true) == ExtSource::mono);
    CHECK (rotor::extSourceFor (4, true) == ExtSource::mono);
}

namespace
{
    // A voice after one modulation tick with the given settings.
    rotor::Voice voiceWith (int index, const rotor::Voice::Parameters& p, const Expression& e, int note = 60)
    {
        rotor::Voice v;
        v.prepare (192000.0);
        v.setIdentity (index, 777 + (std::uint32_t) index);
        v.setExpression (e);
        v.setParameters (p);
        v.noteOn (note, 1.0f);
        for (int i = 0; i < 16; ++i)
            v.process();
        return v;
    }
} // namespace

TEST_CASE ("Pitch bend and Global Detune shift every voice equally", "[expression][voice]")
{
    rotor::Voice::Parameters p;
    p.globalDetune = -0.5;
    Expression e;
    e.bendSemitones = 2.0;
    for (int i = 0; i < 5; ++i)
        CHECK (voiceWith (i, p, e).getPitchOffsetSemitones() == Catch::Approx (2.0 - 0.5 * perf::globalDetuneMaxSemitones));
}

TEST_CASE ("Pitch Drift: in tune at each voice's centre, drifting further away", "[expression][voice]")
{
    rotor::Voice::Parameters p;
    p.pitchDrift = 1.0;
    for (int i = 0; i < 5; ++i)
    {
        const int centre = (int) perf::driftCentreNotes[i];
        CHECK (voiceWith (i, p, {}, centre).getPitchOffsetSemitones() == Catch::Approx (0.0).margin (1e-9));
        const double near = std::abs (voiceWith (i, p, {}, centre + 12).getPitchOffsetSemitones());
        const double far = std::abs (voiceWith (i, p, {}, centre + 36).getPitchOffsetSemitones());
        CHECK (far == Catch::Approx (3.0 * near).epsilon (0.001));
        CHECK (near <= perf::driftMaxCentsPerOctave / 100.0);
    }
    // Voices drift differently (their own tracking error).
    const double a = voiceWith (0, p, {}, 84).getPitchOffsetSemitones();
    const double b = voiceWith (3, p, {}, 84).getPitchOffsetSemitones();
    CHECK (a != b);
}

TEST_CASE ("Wildcard AT down: harmonic clusters move voices 1, 2, 4, 5; voice 3 stays", "[expression][voice]")
{
    rotor::Voice::Parameters p;
    p.atWildcard = -1.0;
    Expression pressed;
    pressed.pressure = 1.0;
    for (int i = 0; i < 5; ++i)
        CHECK (voiceWith (i, p, pressed).getPitchOffsetSemitones() == Catch::Approx (perf::clusterSemitones[i]));
    CHECK (voiceWith (2, p, pressed).getPitchOffsetSemitones() == 0.0);
    // No pressure, no cluster.
    CHECK (voiceWith (0, p, {}).getPitchOffsetSemitones() == 0.0);
}

TEST_CASE ("Wildcard AT up: pressure blends in the pitch wildcards", "[expression][voice]")
{
    rotor::Voice::Parameters p;
    p.atWildcard = 1.0; // wildcard sliders themselves at 0
    Expression pressed;
    pressed.pressure = 1.0;
    CHECK (voiceWith (1, p, {}).getPitchOffsetCents() == 0.0);
    CHECK (voiceWith (1, p, pressed).getPitchOffsetCents() != 0.0); // note detune now active
}

TEST_CASE ("Filter cutoff AT and MPE timbre move the cutoff", "[expression][voice]")
{
    rotor::Voice::Parameters p;
    p.cutoffHz = 1000.0;
    p.atCutoff = 1.0;
    Expression pressed;
    pressed.pressure = 0.5;
    CHECK (voiceWith (0, p, pressed).getFilterCutoff() == Catch::Approx (1000.0 * std::exp2 (0.5 * perf::atCutoffOctaves)));
    p.atCutoff = -1.0;
    CHECK (voiceWith (0, p, pressed).getFilterCutoff() == Catch::Approx (1000.0 * std::exp2 (-0.5 * perf::atCutoffOctaves)));

    rotor::Voice::Parameters q;
    q.cutoffHz = 1000.0;
    Expression bright;
    bright.timbre = 1.0;
    CHECK (voiceWith (0, q, bright).getFilterCutoff() == Catch::Approx (1000.0 * std::exp2 (perf::timbreCutoffOctaves)));
}

TEST_CASE ("LFO rate AT speeds up or slows the LFO", "[expression][voice]")
{
    rotor::Voice::Parameters p;
    p.lfo.rateHz = 2.0;
    p.atLfoRate = 1.0;
    Expression pressed;
    pressed.pressure = 1.0;
    CHECK (voiceWith (0, p, pressed).getLfoFrequency() == Catch::Approx (2.0 * std::exp2 (perf::atLfoRateOctaves)));
    p.atLfoRate = -1.0;
    CHECK (voiceWith (0, p, pressed).getLfoFrequency() == Catch::Approx (2.0 * std::exp2 (-perf::atLfoRateOctaves)));
}

TEST_CASE ("Mod wheel: Pitch LFO mode adds vibrato; Wildcards mode blends wildcards", "[expression][voice]")
{
    rotor::Voice::Parameters p;
    p.lfo.rateHz = 5.0;
    p.wheelPitchLfo = 1.0;
    rotor::Voice v;
    v.prepare (192000.0);
    v.setIdentity (0, 1);
    v.setParameters (p);
    v.noteOn (60, 1.0f);
    double lo = 0.0, hi = 0.0;
    for (int i = 0; i < 192000 / 2; ++i)
    {
        v.process();
        lo = std::min (lo, v.getPitchOffsetSemitones());
        hi = std::max (hi, v.getPitchOffsetSemitones());
    }
    CHECK (hi == Catch::Approx (perf::pitchLfoMaxSemitones).margin (0.02));
    CHECK (lo == Catch::Approx (-perf::pitchLfoMaxSemitones).margin (0.02));

    rotor::Voice::Parameters w;
    w.wheelWildcardBlend = 1.0;
    CHECK (voiceWith (0, w, {}).getPitchOffsetCents() != 0.0);
}
