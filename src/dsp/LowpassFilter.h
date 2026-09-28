#pragma once

#include <algorithm>
#include <cmath>

namespace rotor
{

// Simple v1.1 lowpass: 2-pole TPT state-variable filter (Zavalishin).
// Replaced in v1.4 by the 24 dB ladder + bandpass blend with resonance drive.
class LowpassFilter
{
public:
    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        update();
    }

    // resonance 0..1 (1 = very high Q, still stable).
    void setParameters (double cutoffHz, double resonance)
    {
        cutoff = cutoffHz;
        res = std::clamp (resonance, 0.0, 1.0);
        update();
    }

    void reset() { ic1 = ic2 = 0.0; }

    float process (float input)
    {
        const double v0 = input;
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return static_cast<float> (v2);
    }

private:
    void update()
    {
        const double nyquistSafe = 0.49 * sampleRate;
        const double fc = std::clamp (cutoff, 10.0, nyquistSafe);
        const double g = std::tan (3.141592653589793 * fc / sampleRate);
        const double q = 0.5 + res * 19.5; // Q from 0.5 to 20
        const double k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double sampleRate = 44100.0;
    double cutoff = 1000.0;
    double res = 0.0;
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double ic1 = 0.0, ic2 = 0.0;
};

} // namespace rotor
