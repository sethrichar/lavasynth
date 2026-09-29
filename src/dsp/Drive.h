#pragma once

#include "FastMath.h"
#include "Oversampling.h"

#include <algorithm>
#include <cmath>

namespace rotor
{

// CMOS drive: a model of CMOS-inverter clipping (the 4049-style "fuzz"): asymmetric, sharp,
// and dynamic (the bias sags with the signal level, so the harmonic mix breathes).
// Runs 4× oversampled. Amount = input gain; Mix = dry/wet (mixed inside the oversampled
// domain so dry and wet stay time-aligned). Color (bipolar) sweeps a mid-scoop notch after
// the clipper — sweeping it sounds phaser-like; centre = no scoop.
class CmosDrive
{
public:
    // OPEN: drive range, asymmetry, sag, scoop depth/range — tune by ear.
    static constexpr double maxGainDb = 36.0;
    static constexpr double scoopMaxDepthDb = 15.0;
    static constexpr double scoopCentreHz = 1000.0;
    static constexpr double scoopOctavesEachWay = 2.5;

    void prepare (double newSampleRate)
    {
        baseRate = newSampleRate;
        osRate = baseRate * 4.0;
        sagCoef = 1.0 - std::exp (-1.0 / (0.02 * osRate)); // ~20 ms envelope follower
        dcCoef = 1.0 - std::exp (-2.0 * 3.141592653589793 * 10.0 / osRate);
        reset();
    }

    void reset()
    {
        up.reset();
        down.reset();
        sag = dcState = 0.0;
        z1 = z2 = 0.0;
    }

    void setParameters (double amount, double color, double newMix)
    {
        gain = std::pow (10.0, std::clamp (amount, 0.0, 1.0) * maxGainDb / 20.0);
        // Rough loudness makeup so Amount changes tone more than level (measured on the synth's
        // typical output level; within ~±2 dB across the range).
        makeup = 0.55 + 1.6 / gain;
        mix = std::clamp (newMix, 0.0, 1.0);

        const double c = std::clamp (color, -1.0, 1.0);
        const double hz = scoopCentreHz * std::exp2 (c * scoopOctavesEachWay);
        setScoop (hz, -scoopMaxDepthDb * std::abs (c), 1.4);
    }

    float process (float x)
    {
        float in[4], out[4];
        up.process (x, in);
        for (int i = 0; i < 4; ++i)
        {
            const double dry = in[i];
            double wet = shape (dry * gain);
            wet = scoop (wet);
            out[i] = (float) ((1.0 - mix) * dry + mix * wet * wetLevel * makeup);
        }
        return down.process (out);
    }

    // Base-rate samples of latency through the oversampling filters (dry and wet alike).
    static constexpr double latencySamples() { return 37.75; } // measured (≈ 0.8 ms at 48 kHz)

private:
    static constexpr double wetLevel = 0.35;

    double shape (double x)
    {
        // Dynamic bias: like a CMOS stage's coupling capacitor charging through the asymmetric
        // rails, the operating point shifts in proportion to the signal level. That moves the
        // clipping duty cycle, so even harmonics survive even when both sides clip hard.
        sag += sagCoef * (std::abs (x) - sag);
        const double bias = 0.3 * sag;
        const double v = x + bias;

        // CMOS inverter: steep, asymmetric rails (the positive side clips harder and lower).
        const double y = v > 0.0 ? 0.65 * fastTanh (2.2 * v / 0.65) : fastTanh (1.2 * v);

        // Remove the DC the bias and asymmetry leave behind.
        dcState += dcCoef * (y - dcState);
        return y - dcState;
    }

    // RBJ peaking EQ used as a notch-like scoop.
    void setScoop (double hz, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * 3.141592653589793 * std::min (hz, 0.45 * osRate) / osRate;
        const double alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = -2.0 * std::cos (w) / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = b1;
        a2 = (1.0 - alpha / A) / a0;
    }

    double scoop (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    Upsampler4x up;
    Decimator4x down;
    double baseRate = 48000.0, osRate = 192000.0;
    double gain = 1.0, mix = 0.0, makeup = 2.15;
    double sag = 0.0, sagCoef = 0.0;
    double dcState = 0.0, dcCoef = 0.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;
};

} // namespace rotor
