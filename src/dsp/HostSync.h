#pragma once

#include <array>
#include <cmath>

namespace rotor
{

// Host sync (ENV CLK / LFO CLK): snap a time to the nearest musical note value at the host tempo.
// Note values from 1/64 to 16 bars (4/4), each straight, triplet (×2/3) and dotted (×1.5).
struct NoteValues
{
    // Lengths in quarter-note beats, straight values.
    static constexpr std::array<double, 11> straightBeats { 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8, 16, 32, 64 };

    // Returns the note length (seconds) closest in ratio to `seconds`.
    static double snapSeconds (double seconds, double bpm)
    {
        const double beat = 60.0 / (bpm > 0.0 ? bpm : 120.0);
        double best = straightBeats[0] * beat;
        double bestDistance = 1e300;
        for (double b : straightBeats)
        {
            for (double factor : { 1.0, 2.0 / 3.0, 1.5 })
            {
                const double candidate = b * factor * beat;
                const double distance = std::abs (std::log (candidate / seconds));
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = candidate;
                }
            }
        }
        return best;
    }
};

} // namespace rotor
