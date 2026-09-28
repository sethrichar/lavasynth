#pragma once

#include <algorithm>

namespace rotor
{

// tanh via Lambert's continued fraction (7th order), clamped. Max error: 1e-6 for |x| ≤ 3,
// 1e-4 everywhere (−80 dB). Much cheaper than std::tanh in the per-sample voice path.
inline double fastTanh (double x)
{
    x = std::clamp (x, -5.0, 5.0);
    const double x2 = x * x;
    const double num = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
    const double den = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + 28.0 * x2));
    return std::clamp (num / den, -1.0, 1.0);
}

} // namespace rotor
