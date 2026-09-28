#pragma once

#include "FastMath.h"

#include <algorithm>
#include <cmath>

namespace rotor
{

// OSC Level: 0..0.5 is a clean gain of 0..1 on the main oscillator; above the midpoint
// the level stays at 1 and the oscillator mix is driven into a soft saturator.
struct OscLevel
{
    static double mainGain (double level) { return std::clamp (level, 0.0, 0.5) * 2.0; }
    static double drive (double level) { return std::clamp (level - 0.5, 0.0, 0.5) * 2.0; }
};

// OPEN: overdrive curve and maximum gain (1 + 4 = +14 dB into tanh at full OSC Level).
inline float oscOverdrive (float x, double drive)
{
    if (drive <= 0.0)
        return x;
    const double k = 1.0 + 4.0 * drive;
    return static_cast<float> ((1.0 - drive) * x + drive * fastTanh (k * x));
}

// West-coast style sine wavefolder. amount 0 = exact bypass; the folded signal fades in
// over the first 20% of the range, then more gain means more folds.
// OPEN: fold depth (1 + 6 = up to ~3.5 folds on a full-scale input).
inline float wavefold (float x, double amount)
{
    if (amount <= 0.0)
        return x;
    const double wet = std::min (1.0, amount * 5.0);
    const double gain = 1.0 + 6.0 * amount;
    const double folded = std::sin (1.5707963267948966 * gain * x);
    return static_cast<float> ((1.0 - wet) * x + wet * folded);
}

} // namespace rotor
