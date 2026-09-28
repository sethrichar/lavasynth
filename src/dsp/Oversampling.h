#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace rotor
{

// Linear-phase halfband FIR (Kaiser-windowed sinc) that halves the sample rate.
// Fixed-size storage: no allocation after construction.
template <int NumTaps>
class HalfbandDecimator
{
    static_assert (NumTaps % 4 == 3, "halfband length must be 4m + 3");

public:
    explicit HalfbandDecimator (double kaiserBeta = 8.0)
    {
        const int centre = (NumTaps - 1) / 2;
        double sum = 0.0;
        for (int n = 0; n < NumTaps; ++n)
        {
            const int m = n - centre;
            const double sinc = m == 0 ? 0.5 : std::sin (0.5 * pi * m) / (pi * m);
            const double r = (double) m / centre;
            const double w = besselI0 (kaiserBeta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (kaiserBeta);
            taps[(std::size_t) n] = (m != 0 && m % 2 == 0) ? 0.0 : sinc * w;
            sum += taps[(std::size_t) n];
        }
        for (auto& t : taps)
            t /= sum;
    }

    void reset()
    {
        history = {};
        pos = 0;
    }

    // Two input samples in, one output sample out.
    float process (float a, float b)
    {
        push (a);
        push (b);
        double y = 0.0;
        std::size_t idx = pos;
        for (int n = 0; n < NumTaps; ++n)
        {
            idx = idx == 0 ? (std::size_t) NumTaps - 1 : idx - 1;
            y += taps[(std::size_t) n] * history[idx];
        }
        return static_cast<float> (y);
    }

    static constexpr int latencyInputSamples() { return (NumTaps - 1) / 2; }

private:
    static constexpr double pi = 3.141592653589793;

    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1e-12 * sum)
                break;
        }
        return sum;
    }

    void push (float x)
    {
        history[pos] = x;
        pos = pos + 1 == (std::size_t) NumTaps ? 0 : pos + 1;
    }

    std::array<double, (std::size_t) NumTaps> taps {};
    std::array<double, (std::size_t) NumTaps> history {};
    std::size_t pos = 0;
};

// 4× → 1× decimator: two halfband stages. The voices run at 4× so the nonlinear stages
// (OSC overdrive, filter saturation, wavefolder) alias far less.
class Decimator4x
{
public:
    static constexpr int factor = 4;

    void reset()
    {
        first.reset();
        second.reset();
    }

    float process (const float* in)
    {
        const float a = first.process (in[0], in[1]);
        const float b = first.process (in[2], in[3]);
        return second.process (a, b);
    }

private:
    HalfbandDecimator<31> first { 7.0 };   // 4× → 2×: wide transition is fine up here
    HalfbandDecimator<63> second { 8.0 };  // 2× → 1×: flat to ~20 kHz at 48 kHz
};

} // namespace rotor
