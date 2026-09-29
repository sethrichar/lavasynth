#pragma once

#include <cmath>

namespace rotor
{

// Control-signal wavefolding: when base + modulation leaves [lo, hi] it reflects back in
// instead of clipping (used for the phase distortion, spreader and wavefolder destinations).
inline double foldIntoRange (double x, double lo, double hi)
{
    const double span = hi - lo;
    if (span <= 0.0)
        return lo;
    const double period = 2.0 * span;
    double t = std::fmod (x - lo, period);
    if (t < 0.0)
        t += period;
    if (t > span)
        t = period - t;
    return lo + t;
}

} // namespace rotor
