#pragma once

#include <algorithm>
#include <array>

namespace rotor
{

// LFO min-maxing for the global stereo destinations: the five voice LFOs are combined —
// slider up → the maximum of the five outputs, slider down → the minimum — scaled by the
// slider's distance from centre. Because the LFO rates differ (key tracking), the result is
// complex, non-repeating movement.
inline double lfoMinMax (const std::array<float, 5>& lfos, double depth)
{
    if (depth > 0.0)
        return depth * *std::max_element (lfos.begin(), lfos.end());
    if (depth < 0.0)
        return -depth * *std::min_element (lfos.begin(), lfos.end());
    return 0.0;
}

} // namespace rotor
