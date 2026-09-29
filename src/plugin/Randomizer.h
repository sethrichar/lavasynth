#pragma once

// "Roll the dice": randomize a row (or all three) into something playable, not noise.
// Pure C++ (no JUCE), seeded, so it can be unit-tested. Values are in real parameter units.
//   - Every value stays inside its parameter's range.
//   - Extremes are rarer than useful middles; many modulation depths / wildcards / effects
//     are left at zero most of the time so a roll doesn't turn to mush.
//   - At least one voice is always on.
// Global options (MPE, tune, sync, EXT, master…) are never randomized.

#include "PresetRows.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rotor::presets
{

class Randomizer
{
public:
    explicit Randomizer (std::uint32_t seed = 0x1234567u) : state (seed != 0 ? seed : 1u) {}

    // Random values for every listed parameter that belongs to `row` (row = global → nothing).
    // Parameters without a specific rule get std::nullopt in `fallbacks` so the caller can use a
    // plain uniform value across the parameter's own range.
    Snapshot roll (Row row, const std::vector<std::string>& ids, std::vector<std::string>* fallbacks = nullptr)
    {
        Snapshot out;
        for (const auto& id : ids)
        {
            if (rowOf (id) != row)
                continue;
            if (auto v = valueFor (id))
                out.emplace_back (id, *v);
            else if (fallbacks != nullptr)
                fallbacks->push_back (id);
        }
        if (row == Row::voices)
            ensureAVoiceIsOn (out);
        return out;
    }

    double uniform() // 0..1
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (double) state / 4294967296.0;
    }

private:
    double range (double lo, double hi) { return lo + (hi - lo) * uniform(); }
    double logRange (double lo, double hi) { return lo * std::pow (hi / lo, uniform()); }
    bool chance (double p) { return uniform() < p; }
    float pick (int n) { return (float) std::min (n - 1, (int) (uniform() * n)); }
    // Zero most of the time, otherwise ± up to `depth`.
    float sometimesBipolar (double pZero, double depth) { return chance (pZero) ? 0.0f : (float) range (-depth, depth); }
    float sometimes (double pZero, double lo, double hi) { return chance (pZero) ? 0.0f : (float) range (lo, hi); }

    static bool startsWith (const std::string& s, const char* prefix) { return s.rfind (prefix, 0) == 0; }
    static bool endsWith (const std::string& s, const std::string& suffix)
    {
        return s.size() >= suffix.size() && s.compare (s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    std::optional<float> valueFor (const std::string& id)
    {
        // ---- Row 1: voices + global section --------------------------------------------------
        if (id == "voiceMode") return chance (0.7) ? pick (3) : 3.0f + pick (3); // mostly round-robin
        if (startsWith (id, "voice") && endsWith (id, "On")) return chance (0.85) ? 1.0f : 0.0f;
        if (startsWith (id, "voice") && endsWith (id, "Level")) return (float) range (0.45, 1.0);
        if (startsWith (id, "voice") && endsWith (id, "Octave"))
        {
            const double u = uniform(); // mostly −1…+1
            return u < 0.1 ? -2.0f : u < 0.3 ? -1.0f : u < 0.75 ? 0.0f : u < 0.93 ? 1.0f : 2.0f;
        }
        if (startsWith (id, "voice") && endsWith (id, "Waveform")) return pick (5);
        if (id == "oscLevel") return (float) range (0.3, 0.7);
        if (id == "subLevel") return sometimes (0.5, 0.2, 0.7);
        if (id == "subOctave" || id == "subWaveform") return pick (2);
        if (id == "noiseLevel") return sometimes (0.65, 0.05, 0.35);
        if (id == "noiseColor") return (float) range (-0.6, 0.6);
        if (id == "glide") return chance (0.7) ? 0.0f : (float) logRange (0.02, 0.4);
        if (id == "phaseDist") return sometimesBipolar (0.6, 0.6);

        // ---- Row 2: aftertouch, envelopes, filter ------------------------------------------
        if (id == "atWildcard" || id == "atCutoff" || id == "atLfoRate") return sometimesBipolar (0.5, 0.7);
        if (id == "attack" || id == "modAttack") return chance (0.6) ? 0.0153f : (float) logRange (0.02, 2.5);
        if (id == "decay" || id == "modDecay") return (float) logRange (0.08, 3.0);
        if (id == "sustain" || id == "modSustain") return (float) range (0.0, 1.0);
        if (id == "release" || id == "modRelease") return (float) logRange (0.05, 4.0);
        if (id == "ampLoop") return chance (0.1) ? 1.0f : 0.0f;
        if (id == "modLoop") return chance (0.2) ? 1.0f : 0.0f;
        if (id == "ampKeyTrack" || id == "modKeyTrack") return sometimesBipolar (0.6, 0.6);
        if (startsWith (id, "modEnvTo")) return sometimesBipolar (0.5, 0.6);
        if (id == "cutoff") return (float) logRange (250.0, 9000.0);
        if (id == "resonance") return (float) range (0.0, 0.8);
        if (id == "filterMode") return chance (0.8) ? 0.0f : 1.0f;
        if (id == "keyTrack") return (float) range (0.0, 1.0);
        if (id == "envCurve" || id == "filterCharacter") return pick (5);

        // ---- Row 3: LFO, effects, wildcards ------------------------------------------------
        if (id == "lfoRate") return (float) range (0.1, 0.9);
        if (id == "lfoRange") return chance (0.7) ? 0.0f : 1.0f;
        if (id == "lfoShape") return pick (5);
        if (id == "lfoKeyTrack") return sometimesBipolar (0.6, 0.6);
        if (startsWith (id, "lfoTo")) return sometimesBipolar (0.6, 0.5);
        if (id == "reverbAmount") return (float) range (0.2, 0.9);
        if (id == "reverbMix") return sometimes (0.25, 0.1, 0.5);
        if (id == "driveAmount") return (float) range (0.1, 0.7);
        if (id == "driveMix") return sometimes (0.5, 0.1, 0.5);
        if (id == "fxColor") return (float) range (-0.5, 0.5);
        if (id == "spread") return (float) range (0.2, 1.0);
        if (id == "noteDetune" || id == "wow" || id == "flutter" || id == "reelDrag" || id == "chaos"
            || id == "envScatter" || id == "fold")
            return sometimes (0.6, 0.05, 0.5);

        return std::nullopt;
    }

    static void ensureAVoiceIsOn (Snapshot& values)
    {
        bool anyOn = false, sawVoice = false;
        for (const auto& [id, v] : values)
            if (id.rfind ("voice", 0) == 0 && endsWith (id, "On"))
            {
                sawVoice = true;
                anyOn = anyOn || v >= 0.5f;
            }
        if (sawVoice && ! anyOn)
            for (auto& [id, v] : values)
                if (id == "voice1On")
                    v = 1.0f;
    }

    std::uint32_t state;
};

} // namespace rotor::presets
