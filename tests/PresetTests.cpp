#include "plugin/PresetRows.h"

#include <catch2/catch_test_macros.hpp>

using namespace rotor::presets;

TEST_CASE ("Parameters sort into the manual's three rows", "[presets]")
{
    // Row 1: voices + global section
    for (auto id : { "voiceMode", "voice1On", "voice3Level", "voice5Waveform", "oscLevel", "subOctave", "noiseColor", "glide" })
        CHECK (rowOf (id) == Row::voices);
    // Row 2: aftertouch, envelopes, filter (+ voicing selectors)
    for (auto id : { "atWildcard", "atLfoRate", "attack", "release", "ampLoop", "modAttack", "modEnvToCutoff",
                     "cutoff", "resonance", "filterMode", "keyTrack", "envCurve", "filterCharacter" })
        CHECK (rowOf (id) == Row::shaping);
    // Row 3: LFO, effects, wildcards
    for (auto id : { "lfoRate", "lfoShape", "lfoToFxColor", "reverbMix", "driveAmount", "fxColor", "wow", "spread", "fold" })
        CHECK (rowOf (id) == Row::motion);
    // Global options: never in a row
    for (auto id : { "lfoRetrigger", "lfoSync", "envSync", "mpe", "tune", "tuneMode", "extInput", "level", "roundRobinReset" })
        CHECK (rowOf (id) == Row::global);
    CHECK (rowOf ("somethingNew") == Row::unknown);
}

TEST_CASE ("Row store/recall keeps only that row's parameters", "[presets]")
{
    RowPresetBank bank;
    const Snapshot panel { { "cutoff", 500.0f }, { "voiceMode", 3.0f }, { "wow", 0.4f }, { "mpe", 1.0f } };
    CHECK_FALSE (bank.isFilled (Row::shaping, 2));
    CHECK (bank.recall (Row::shaping, 2).empty());

    bank.store (Row::shaping, 2, panel);
    CHECK (bank.isFilled (Row::shaping, 2));
    CHECK (bank.recall (Row::shaping, 2) == Snapshot { { "cutoff", 500.0f } });
    CHECK_FALSE (bank.isFilled (Row::voices, 2)); // other rows untouched
}

TEST_CASE ("FULL store/recall covers all three rows but never the global options", "[presets]")
{
    RowPresetBank bank;
    const Snapshot panel { { "cutoff", 500.0f }, { "voiceMode", 3.0f }, { "wow", 0.4f }, { "mpe", 1.0f }, { "level", 0.3f } };
    bank.storeFull (7, panel);
    for (int r = 0; r < numRows; ++r)
        CHECK (bank.isFilled (static_cast<Row> (r), 7));
    const auto all = bank.recallFull (7);
    CHECK (all.size() == 3);
    for (const auto& [id, v] : all)
        CHECK (rowOf (id) != Row::global);

    // Mixing: row 1 from slot 7 and row 3 from another slot is just two row recalls.
    bank.store (Row::motion, 0, { { "wow", 0.9f } });
    CHECK (bank.recall (Row::motion, 0) == Snapshot { { "wow", 0.9f } });
    CHECK (bank.recall (Row::motion, 7) == Snapshot { { "wow", 0.4f } });
}

TEST_CASE ("Slot bounds are safe", "[presets]")
{
    RowPresetBank bank;
    bank.store (Row::voices, 8, { { "glide", 1.0f } });
    bank.store (Row::global, 0, { { "mpe", 1.0f } });
    CHECK (bank.recall (Row::voices, 8).empty());
    CHECK (bank.recall (Row::voices, -1).empty());
    CHECK (bank.recall (Row::global, 0).empty());
    bank.set (Row::voices, 1, { { "glide", 0.5f } });
    bank.clear (Row::voices, 1);
    CHECK_FALSE (bank.isFilled (Row::voices, 1));
}

#include "plugin/Randomizer.h"

namespace
{
    // Every parameter ID the plugin has (kept in sync by RotorRender --check-presets, which checks
    // the real list has no unclassified IDs; this list only needs to cover each rule).
    std::vector<std::string> allIds()
    {
        std::vector<std::string> ids { "voiceMode", "oscLevel", "subLevel", "subOctave", "subWaveform", "noiseLevel",
                                       "noiseColor", "glide", "phaseDist", "atWildcard", "atCutoff", "atLfoRate", "attack",
                                       "decay", "sustain", "release", "ampLoop", "ampKeyTrack", "envCurve", "modAttack",
                                       "modDecay", "modSustain", "modRelease", "modLoop", "modKeyTrack", "modEnvToPd",
                                       "modEnvToCutoff", "modEnvToLfoRate", "modEnvToSpread", "modEnvToFold", "filterMode",
                                       "cutoff", "resonance", "keyTrack", "filterCharacter", "lfoRate", "lfoRange",
                                       "lfoShape", "lfoKeyTrack", "lfoToPd", "lfoToCutoff", "lfoToSpread", "lfoToFold",
                                       "lfoToReverbMix", "lfoToDriveMix", "lfoToFxAmount", "lfoToFxColor", "reverbAmount",
                                       "reverbMix", "driveAmount", "driveMix", "fxColor", "noteDetune", "wow", "flutter",
                                       "reelDrag", "chaos", "envScatter", "spread", "fold", "mpe", "tune", "level",
                                       "envSync", "lfoSync", "lfoRetrigger", "extInput" };
        for (int v = 1; v <= 5; ++v)
            for (auto suffix : { "On", "Level", "Octave", "Waveform" })
                ids.push_back ("voice" + std::to_string (v) + suffix);
        return ids;
    }
} // namespace

TEST_CASE ("Randomize touches only its row, never the global options", "[presets][random]")
{
    Randomizer r (42);
    for (int row = 0; row < numRows; ++row)
    {
        std::vector<std::string> fallbacks;
        const auto values = r.roll (static_cast<Row> (row), allIds(), &fallbacks);
        CHECK_FALSE (values.empty());
        CHECK (fallbacks.empty()); // every known parameter has a rule
        for (const auto& [id, v] : values)
            CHECK (rowOf (id) == static_cast<Row> (row));
    }
    CHECK (r.roll (Row::global, allIds()).empty());
}

TEST_CASE ("Randomize keeps values in range and at least one voice on", "[presets][random]")
{
    Randomizer r (7);
    for (int i = 0; i < 500; ++i)
    {
        const auto voices = r.roll (Row::voices, allIds());
        int on = 0;
        for (const auto& [id, v] : voices)
        {
            if (id.size() > 7 && id.rfind ("voice", 0) == 0 && id.substr (id.size() - 2) == "On")
                on += v >= 0.5f ? 1 : 0;
            if (id.find ("Octave") != std::string::npos) { REQUIRE (v >= -2.0f); REQUIRE (v <= 2.0f); }
            if (id.find ("Waveform") != std::string::npos || id == "voiceMode") { REQUIRE (v >= 0.0f); REQUIRE (v <= 5.0f); }
        }
        REQUIRE (on >= 1);

        for (const auto& [id, v] : r.roll (Row::shaping, allIds()))
        {
            if (id == "cutoff") { REQUIRE (v >= 20.0f); REQUIRE (v <= 20000.0f); }
            if (id == "resonance" || id == "sustain" || id == "modSustain" || id == "keyTrack") { REQUIRE (v >= 0.0f); REQUIRE (v <= 1.0f); }
            if (id.rfind ("modEnvTo", 0) == 0 || id == "atWildcard" || id == "atCutoff" || id == "atLfoRate") { REQUIRE (v >= -1.0f); REQUIRE (v <= 1.0f); }
            if (id == "attack" || id == "decay" || id == "release") { REQUIRE (v >= 0.0153f); REQUIRE (v <= 60.0f); }
        }
        for (const auto& [id, v] : r.roll (Row::motion, allIds()))
        {
            REQUIRE (v >= -1.0f);
            REQUIRE (v <= 4.0f);
        }
    }
}

TEST_CASE ("Randomize is reproducible from a seed and varies between rolls", "[presets][random]")
{
    Randomizer a (99), b (99);
    CHECK (a.roll (Row::motion, allIds()) == b.roll (Row::motion, allIds()));
    CHECK (a.roll (Row::motion, allIds()) != a.roll (Row::motion, allIds()));
}
