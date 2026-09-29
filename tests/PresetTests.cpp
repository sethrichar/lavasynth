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
