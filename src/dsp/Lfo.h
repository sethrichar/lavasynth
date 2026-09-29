#pragma once

#include "Oscillator.h"

#include <cmath>
#include <cstdint>

namespace rotor
{

// Per-voice LFO, bipolar output (-1..+1). Square and saws are band-limited (it can run at
// audio rate: fast range + full key tracking).
class Lfo
{
public:
    // Panel order, top to bottom.
    enum class Shape
    {
        volcano = 0, // slewed sample-and-hold, two new random values per cycle
        square,
        reverseSaw,  // ramps down
        saw,         // ramps up
        sine
    };

    enum class Range
    {
        slow = 0,
        fast
    };

    // Rate ranges from the manual (Slow 0.064–4.8 Hz, Fast 1.02–65.4 Hz). The ends that are
    // Cs are set exactly: C2 = 65.406 Hz, C−4 = C2 / 64, C−8 = C2 / 1024
    // (slow minimum to fast maximum = the manual's "10 octaves total").
    static constexpr double fastMaxHz = 65.406391325149658;
    static constexpr double fastMinHz = fastMaxHz / 64.0;
    static constexpr double slowMinHz = fastMaxHz / 1024.0;
    static constexpr double slowMaxHz = 4.8;

    // Rate knob 0..1 → Hz, exponential across the range.
    static double rateToHz (double knob, Range range)
    {
        const double lo = range == Range::slow ? slowMinHz : fastMinHz;
        const double hi = range == Range::slow ? slowMaxHz : fastMaxHz;
        return lo * std::pow (hi / lo, knob);
    }

    explicit Lfo (std::uint32_t seed = 0x2545F491u) : rng (seed != 0 ? seed : 1u) {}

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        osc.setSampleRate (sampleRate);
        setFrequency (frequency);
    }

    void setShape (Shape newShape)
    {
        shape = newShape;
        switch (shape)
        {
            case Shape::square: osc.setWaveform (Waveform::square); break;
            case Shape::reverseSaw:
            case Shape::saw: osc.setWaveform (Waveform::saw); break;
            case Shape::sine: osc.setWaveform (Waveform::sine); break;
            case Shape::volcano: break;
        }
    }

    void setFrequency (double hz)
    {
        frequency = hz;
        osc.setFrequency (hz);
        increment = std::min (hz / sampleRate, 0.49);
    }

    double getFrequency() const { return frequency; }

    // Retrigger: restart the cycle.
    void reset()
    {
        osc.reset();
        phase = 0.0;
    }

    float process()
    {
        double y = 0.0;
        switch (shape)
        {
            case Shape::volcano: y = volcano(); osc.process(); break;
            case Shape::square: y = osc.process(); break;
            case Shape::saw: y = osc.process(); break;
            case Shape::reverseSaw: y = -osc.process(); break;
            case Shape::sine: y = osc.process(); break;
        }
        // Keep a phase for Volcano in step with the oscillator.
        phase += increment;
        if (phase >= 1.0) phase -= 1.0;

        value = static_cast<float> (y);
        return value;
    }

    float getValue() const { return value; }

private:
    // Two random targets per cycle; each half cycle glides (raised-cosine) from the last
    // value to the next — a sample-and-hold with slew.
    // OPEN: slew amount (here: the full half cycle, i.e. maximally smooth).
    double volcano()
    {
        const int half = phase < 0.5 ? 0 : 1;
        if (half != currentHalf)
        {
            currentHalf = half;
            from = to;
            to = nextRandom();
        }
        const double t = (phase - 0.5 * half) * 2.0; // 0..1 within the half cycle
        const double s = 0.5 - 0.5 * std::cos (3.141592653589793 * t);
        return from + (to - from) * s;
    }

    double nextRandom()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (double) rng / 2147483648.0 - 1.0;
    }

    double sampleRate = 44100.0;
    double frequency = 1.0;
    double increment = 0.0;
    double phase = 0.0;
    Shape shape = Shape::sine;
    Oscillator osc;
    std::uint32_t rng;
    int currentHalf = -1;
    double from = 0.0, to = 0.0;
    float value = 0.0f;
};

} // namespace rotor
