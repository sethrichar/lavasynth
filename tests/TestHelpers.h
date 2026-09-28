#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace testing
{

// In-place radix-2 FFT (size must be a power of two).
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.141592653589793 / (double) len;
        const std::complex<double> wlen (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// Ratio (dB) of energy away from the harmonics of f0 to energy on them.
// Lower = less aliasing.
inline double aliasToHarmonicDb (const std::vector<float>& signal, double f0, double sampleRate)
{
    const size_t n = signal.size();
    std::vector<std::complex<double>> bins (n);
    for (size_t i = 0; i < n; ++i)
    {
        // 4-term Blackman-Harris: -92 dB sidelobes, so window leakage doesn't mask aliasing.
        const double x = 2.0 * 3.141592653589793 * (double) i / (double) (n - 1);
        const double w = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x);
        bins[i] = signal[i] * w;
    }
    fft (bins);

    const double binHz = sampleRate / (double) n;
    double harmonic = 0.0, other = 0.0;
    for (size_t k = 1; k < n / 2; ++k)
    {
        const double f = (double) k * binHz;
        const double nearest = std::round (f / f0) * f0;
        const double e = std::norm (bins[k]);
        if (std::abs (f - nearest) <= 6.0 * binHz)
            harmonic += e;
        else
            other += e;
    }
    return 10.0 * std::log10 (other / harmonic);
}

} // namespace testing
