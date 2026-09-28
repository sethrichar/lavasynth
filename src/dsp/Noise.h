#pragma once

#include <cmath>
#include <cstdint>

namespace rotor
{

// Pink noise: white noise through Paul Kellet's economy 1/f filter.
class PinkNoise
{
public:
    explicit PinkNoise (std::uint32_t seed = 0x12345678u) : rng (seed != 0 ? seed : 1u) {}

    void reset() { b0 = b1 = b2 = 0.0; }

    float process()
    {
        const double white = nextWhite();
        b0 = 0.99765 * b0 + white * 0.0990460;
        b1 = 0.96300 * b1 + white * 0.2965164;
        b2 = 0.57000 * b2 + white * 1.0526913;
        return static_cast<float> ((b0 + b1 + b2 + white * 0.1848) * outputGain);
    }

private:
    static constexpr double outputGain = 0.25;

    double nextWhite()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (double) rng / 2147483648.0 - 1.0; // -1..1
    }

    std::uint32_t rng;
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
};

// Tilt EQ around a pivot: Color > 0 brightens (lows down, highs up), < 0 darkens.
class TiltEq
{
public:
    static constexpr double pivotHz = 800.0;
    // OPEN: tilt range. Each side moves up to this many dB at full Color.
    static constexpr double maxDbPerSide = 6.0;

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        const double g = std::tan (3.141592653589793 * pivotHz / sampleRate);
        G = g / (1.0 + g);
    }

    // color: -1..+1, 0 = flat.
    void setColor (double color)
    {
        const double db = color * maxDbPerSide;
        highGain = std::pow (10.0, db / 20.0);
        lowGain = 1.0 / highGain;
    }

    void reset() { state = 0.0; }

    float process (float x)
    {
        // TPT one-pole split (exact shelf shape up to Nyquist).
        const double v = (x - state) * G;
        const double lp = v + state;
        state = lp + v;
        return static_cast<float> (lowGain * lp + highGain * (x - lp));
    }

private:
    double sampleRate = 44100.0;
    double G = 0.0;
    double state = 0.0;
    double lowGain = 1.0, highGain = 1.0;
};

} // namespace rotor
