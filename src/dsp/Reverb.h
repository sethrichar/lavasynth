#pragma once

#include "Noise.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rotor
{

// Delay line with fractional (linear-interpolated) reads. Memory is sized in prepare(),
// never on the audio thread.
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        buffer.assign ((size_t) maxSamples + 2, 0.0f);
        write = 0;
    }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x)
    {
        buffer[write] = x;
        write = write + 1 == buffer.size() ? 0 : write + 1;
    }

    // Sample written `delay` pushes ago (delay >= 1), linearly interpolated.
    float read (double delay) const
    {
        const int whole = (int) delay;
        const float frac = (float) (delay - whole);
        const float a = tap (whole), b = tap (whole + 1);
        return a + frac * (b - a);
    }

    float tap (int delay) const
    {
        const long pos = (long) write - delay;
        return buffer[(size_t) (pos < 0 ? pos + (long) buffer.size() : pos)];
    }

private:
    std::vector<float> buffer;
    size_t write = 0;
};

// Stereo plate reverb after Jon Dattorro, "Effect Design, Part 1" (JAES 1997) — the classic
// 1980s-digital-plate topology: input diffusers into a figure-eight tank with modulated allpasses.
//
// Amount is a macro (per the manual): low = short, still, narrow; high = long, modulated, wide.
// Color (bipolar): tank damping + a tilt EQ on the wet signal. Mix = dry/wet.
class PlateReverb
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        scale = sampleRate / designRate;
        auto size = [this] (double n) { return (int) std::ceil (n * scale) + 64; };

        preDelay.prepare (size (0.03 * designRate));
        for (int i = 0; i < 4; ++i)
            inputDiffusers[i].prepare (size (inputLengths[i]));
        for (int side = 0; side < 2; ++side)
        {
            tank[side].modAllpass.prepare (size (modAllpassLengths[side] + 40));
            tank[side].delay1.prepare (size (delay1Lengths[side]));
            tank[side].allpass.prepare (size (allpassLengths[side]));
            tank[side].delay2.prepare (size (delay2Lengths[side]));
        }
        tilt.setSampleRate (sampleRate);
        reset();
    }

    void reset()
    {
        preDelay.reset();
        for (auto& d : inputDiffusers) d.reset();
        for (auto& t : tank)
        {
            t.modAllpass.reset();
            t.delay1.reset();
            t.allpass.reset();
            t.delay2.reset();
            t.damp = 0.0;
        }
        bandwidthState = 0.0;
        lfoPhase = 0.0;
        tilt.reset();
        tiltRight.reset();
    }

    // amount 0..1, color -1..+1, mix 0..1
    void setParameters (double amount, double color, double newMix)
    {
        const double a = std::clamp (amount, 0.0, 1.0);
        // OPEN: the macro's ranges.
        decay = 0.25 + 0.72 * a;                      // tail: ~0.5 s … ~10 s
        excursion = (1.0 + 11.0 * a) * scale;         // tank modulation depth (samples)
        width = a;                                    // stereo width of the wet signal
        levelTrim = 1.0 - 0.5 * a;
        const double c = std::clamp (color, -1.0, 1.0);
        damping = 0.35 - 0.3 * c;                     // bright (+) = less high-frequency loss in the tank
        tilt.setColor (0.8 * c);
        tiltRight.setColor (0.8 * c);
        mix = std::clamp (newMix, 0.0, 1.0);
    }

    void process (float& left, float& right)
    {
        const float dryL = left, dryR = right;

        // Input: mono sum → predelay → bandwidth lowpass → 4 diffusers.
        preDelay.push (0.5f * (dryL + dryR));
        double x = preDelay.read (preDelaySeconds * sampleRate);
        bandwidthState += bandwidth * (x - bandwidthState);
        x = bandwidthState;
        x = allpass (inputDiffusers[0], x, inputLengths[0] * scale, inputDiffusion1);
        x = allpass (inputDiffusers[1], x, inputLengths[1] * scale, inputDiffusion1);
        x = allpass (inputDiffusers[2], x, inputLengths[2] * scale, inputDiffusion2);
        x = allpass (inputDiffusers[3], x, inputLengths[3] * scale, inputDiffusion2);

        // Tank: each half feeds the other (figure eight).
        lfoPhase += tankLfoHz / sampleRate;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const double lfo[2] = { std::sin (twoPi * lfoPhase), std::cos (twoPi * lfoPhase) };

        const double feedback[2] = { tank[1].delay2.tap (delay2Samples (1)) * decay,
                                     tank[0].delay2.tap (delay2Samples (0)) * decay };
        for (int side = 0; side < 2; ++side)
        {
            auto& t = tank[side];
            double y = x + feedback[side];
            y = allpass (t.modAllpass, y, modAllpassLengths[side] * scale + excursion * lfo[side], -decayDiffusion1);
            t.delay1.push ((float) y);
            y = t.delay1.tap (delay1Samples (side));
            t.damp += (1.0 - damping) * (y - t.damp);
            y = t.damp * decay;
            y = allpass (t.allpass, y, allpassLengths[side] * scale, decayDiffusion2);
            t.delay2.push ((float) y);
        }

        // Output taps (Dattorro's table).
        const auto& L = tank[0];
        const auto& R = tank[1];
        double wetL = R.delay1.tap (s (266)) + R.delay1.tap (s (2974)) - R.allpass.tap (s (1913)) + R.delay2.tap (s (1996))
                      - L.delay1.tap (s (1990)) - L.allpass.tap (s (187)) - L.delay2.tap (s (1066));
        double wetR = L.delay1.tap (s (353)) + L.delay1.tap (s (3627)) - L.allpass.tap (s (1228)) + L.delay2.tap (s (2673))
                      - R.delay1.tap (s (2111)) - R.allpass.tap (s (335)) - R.delay2.tap (s (121));
        // Longer tails build up more energy; trim so Amount mostly changes the space, not the level.
        wetL *= outputGain * levelTrim;
        wetR *= outputGain * levelTrim;

        // Width: blend toward mono at low amounts.
        const double mid = 0.5 * (wetL + wetR), side = 0.5 * (wetL - wetR) * width;
        wetL = tilt.process ((float) (mid + side));
        wetR = tiltRight.process ((float) (mid - side));

        left = (float) ((1.0 - mix) * dryL + mix * wetL);
        right = (float) ((1.0 - mix) * dryR + mix * wetR);
    }

private:
    static constexpr double designRate = 29761.0; // Dattorro's reference rate
    static constexpr double twoPi = 6.283185307179586;
    static constexpr double inputLengths[4] = { 142, 107, 379, 277 };
    static constexpr double modAllpassLengths[2] = { 672, 908 };
    static constexpr double delay1Lengths[2] = { 4453, 4217 };
    static constexpr double allpassLengths[2] = { 1800, 2656 };
    static constexpr double delay2Lengths[2] = { 3720, 3163 };
    static constexpr double inputDiffusion1 = 0.75, inputDiffusion2 = 0.625;
    static constexpr double decayDiffusion1 = 0.7, decayDiffusion2 = 0.5;
    static constexpr double bandwidth = 0.9995;
    static constexpr double preDelaySeconds = 0.012;
    static constexpr double tankLfoHz = 0.9;
    static constexpr double outputGain = 0.6;

    struct Tank
    {
        DelayLine modAllpass, delay1, allpass, delay2;
        double damp = 0.0;
    };

    int s (double n) const { return std::max (1, (int) std::lround (n * scale)); }
    int delay1Samples (int side) const { return s (delay1Lengths[side]); }
    int delay2Samples (int side) const { return s (delay2Lengths[side]); }

    // Schroeder allpass built on a delay line; `delay` may be fractional (modulated).
    static double allpass (DelayLine& line, double x, double delay, double g)
    {
        const double delayed = line.read (delay);
        const double v = x + g * delayed;
        line.push ((float) v);
        return delayed - g * v;
    }

    double sampleRate = 48000.0, scale = 48000.0 / designRate;
    DelayLine preDelay;
    DelayLine inputDiffusers[4];
    Tank tank[2];
    double bandwidthState = 0.0, lfoPhase = 0.0;
    double levelTrim = 1.0;
    double decay = 0.5, excursion = 1.0, width = 0.5, damping = 0.35, mix = 0.0;
    TiltEq tilt, tiltRight;
};

} // namespace rotor
