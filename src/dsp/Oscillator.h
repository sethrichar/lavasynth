#pragma once

#include <cmath>

namespace rotor
{

// Waveforms in panel order (slider top to bottom = most to least harmonics).
enum class Waveform
{
    square = 0,
    saw,
    sharkTooth,
    triangle,
    sine
};

constexpr int numWaveforms = 5;

// Band-limited oscillator: polyBLEP on the discontinuities (square, saw) and
// polyBLAMP on the slope corners (shark-tooth, triangle).
class Oscillator
{
public:
    // OPEN: exact shark-tooth shape. Modelled as a skewed triangle whose rise takes
    // this fraction of the cycle and whose fall is steep (saw/triangle hybrid).
    static constexpr double sharkToothRise = 0.85;

    void setSampleRate (double newSampleRate) { sampleRate = newSampleRate; }
    void setWaveform (Waveform newWaveform) { waveform = newWaveform; }
    Waveform getWaveform() const { return waveform; }

    void setFrequency (double hz)
    {
        increment = hz / sampleRate;
        if (increment > 0.49) increment = 0.49; // stay below Nyquist; also keeps the BLEP windows valid
        if (increment < 0.0) increment = 0.0;
    }

    void reset (double startPhase = 0.0) { phase = startPhase; }

    float process()
    {
        const double t = phase;
        const double dt = increment;
        double y = 0.0;

        switch (waveform)
        {
            case Waveform::square:
                y = t < 0.5 ? 1.0 : -1.0;
                y += polyBlep (t, dt);
                y -= polyBlep (wrap (t + 0.5), dt);
                break;

            case Waveform::saw:
                y = 2.0 * t - 1.0;
                y -= polyBlep (t, dt);
                break;

            case Waveform::sharkTooth:
                y = skewedTriangle (t, dt, sharkToothRise);
                break;

            case Waveform::triangle:
                y = skewedTriangle (t, dt, 0.5);
                break;

            case Waveform::sine:
                y = std::sin (twoPi * t);
                break;
        }

        phase += dt;
        if (phase >= 1.0) phase -= 1.0;

        return static_cast<float> (y);
    }

    // Residuals are public so tests can check them directly.
    static double polyBlep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    static double polyBlamp (double t, double dt)
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt)
        {
            t = t / dt - 1.0;
            return -t * t * t / 3.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt + 1.0;
            return t * t * t / 3.0;
        }
        return 0.0;
    }

private:
    static constexpr double twoPi = 6.283185307179586;

    static double wrap (double t) { return t >= 1.0 ? t - 1.0 : t; }

    // Rises -1 → +1 over [0, rise), falls +1 → -1 over [rise, 1).
    static double skewedTriangle (double t, double dt, double rise)
    {
        const double up = 2.0 / rise;
        const double down = 2.0 / (1.0 - rise);
        double y = t < rise ? -1.0 + up * t : 1.0 - down * (t - rise);

        // Slope changes by (up + down) per cycle at each corner; this polyBLAMP
        // is normalised so a slope change of s per sample needs a weight of s / 2.
        const double corner = 0.5 * (up + down) * dt;
        y += corner * polyBlamp (t, dt);
        y -= corner * polyBlamp (wrap (t + 1.0 - rise), dt);
        return y;
    }

    double sampleRate = 44100.0;
    double phase = 0.0;
    double increment = 0.0;
    Waveform waveform = Waveform::sharkTooth;
};

} // namespace rotor
