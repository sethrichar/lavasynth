#pragma once

// Row presets (pure C++, no JUCE): the panel has 3 rows, each with 8 preset slots.
//   Row 1: Voices (mode + 5 voices), Global (OSC, Sub, Noise/EXT colour, Glide) — plus PD offset
//   Row 2: Aftertouch, Amp Envelope, Mod Envelope, Filter — plus the voicing selectors
//   Row 3: LFO, Effects, Wildcards
// Global options (the hardware's DIP switches, sync/EXT toggles, master level) are not stored per row.
// OPEN: whether the hardware stores the master-section toggles (EXT, ENV/LFO CLK) in row presets.

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rotor::presets
{

enum class Row
{
    voices = 0, // row 1
    shaping,    // row 2
    motion,     // row 3
    global,     // never stored in a row
    unknown     // a parameter nobody classified — a bug (checked by RotorRender --check-rows)
};

constexpr int numRows = 3;
constexpr int numSlots = 8;

inline bool startsWith (std::string_view s, std::string_view prefix) { return s.substr (0, prefix.size()) == prefix; }

inline Row rowOf (std::string_view id)
{
    auto in = [&id] (std::initializer_list<std::string_view> ids)
    {
        for (auto x : ids)
            if (id == x) return true;
        return false;
    };

    // Global options first (some share prefixes with row parameters, e.g. lfoRetrigger/lfoSync).
    if (in ({ "roundRobinReset", "unisonGrace", "monoPriority", "lfoRetrigger", "envSync", "lfoSync", "mpe",
              "modWheelMode", "tuneMode", "tune", "extInput", "level" }))
        return Row::global;

    if (startsWith (id, "voice") // voiceMode, voice1On … voice5Waveform
        || in ({ "oscLevel", "subLevel", "subOctave", "subWaveform", "noiseLevel", "noiseColor", "glide", "phaseDist" }))
        return Row::voices;

    if (startsWith (id, "at") || startsWith (id, "mod") // aftertouch, mod envelope + depths
        || in ({ "attack", "decay", "sustain", "release", "ampLoop", "ampKeyTrack", "envCurve",
                 "filterMode", "cutoff", "resonance", "keyTrack", "filterCharacter" }))
        return Row::shaping;

    if (startsWith (id, "lfo") || startsWith (id, "reverb") || startsWith (id, "drive")
        || in ({ "fxColor", "noteDetune", "wow", "flutter", "reelDrag", "chaos", "envScatter", "spread", "fold" }))
        return Row::motion;

    return Row::unknown;
}

// Parameter values in real units, keyed by parameter ID.
using Snapshot = std::vector<std::pair<std::string, float>>;

// The 3 × 8 row memories. FULL recall of slot N = rows 1–3 slot N together.
class RowPresetBank
{
public:
    // Stores this row's parameters from a full snapshot of the panel.
    void store (Row row, int slot, const Snapshot& panel)
    {
        if (! valid (row, slot))
            return;
        Snapshot rowOnly;
        for (const auto& [id, value] : panel)
            if (rowOf (id) == row)
                rowOnly.emplace_back (id, value);
        slots[(size_t) row][(size_t) slot] = std::move (rowOnly);
    }

    void storeFull (int slot, const Snapshot& panel)
    {
        for (int r = 0; r < numRows; ++r)
            store (static_cast<Row> (r), slot, panel);
    }

    // The values to apply (empty if that slot was never stored).
    Snapshot recall (Row row, int slot) const
    {
        if (! valid (row, slot) || ! slots[(size_t) row][(size_t) slot])
            return {};
        return *slots[(size_t) row][(size_t) slot];
    }

    Snapshot recallFull (int slot) const
    {
        Snapshot all;
        for (int r = 0; r < numRows; ++r)
            for (auto& v : recall (static_cast<Row> (r), slot))
                all.push_back (std::move (v));
        return all;
    }

    bool isFilled (Row row, int slot) const { return valid (row, slot) && slots[(size_t) row][(size_t) slot].has_value(); }
    void clear (Row row, int slot)
    {
        if (valid (row, slot))
            slots[(size_t) row][(size_t) slot].reset();
    }

    // For saving with the plugin state: set a slot's contents directly.
    void set (Row row, int slot, Snapshot values)
    {
        if (valid (row, slot))
            slots[(size_t) row][(size_t) slot] = std::move (values);
    }

private:
    static bool valid (Row row, int slot)
    {
        return (int) row >= 0 && (int) row < numRows && slot >= 0 && slot < numSlots;
    }

    std::array<std::array<std::optional<Snapshot>, numSlots>, numRows> slots {};
};

} // namespace rotor::presets
