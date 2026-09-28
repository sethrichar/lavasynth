#pragma once

#include "FastMath.h"

#include <algorithm>
#include <cmath>

namespace rotor
{

// Per-voice filter: 4-pole TPT ladder with a tanh in the feedback path (Zavalishin-style,
// feedback solved linearly, then saturated). Runs at the oversampled voice rate.
//
// Lowpass: 24 dB/oct. Resonance pushes the input and feedback into the tanh, so high
//   settings overdrive; at maximum it self-oscillates at the cutoff frequency.
// Bandpass: ladder-tap bandpass plus a little 12 dB/oct lowpass, with hotter resonance.
class LadderFilter
{
public:
    enum class Mode
    {
        lowpass = 0,
        bandpass
    };

    // Feedback gain at resonance = 1. The linear ladder starts to self-oscillate at 4.
    static constexpr double maxFeedback = 4.4;
    // Bandpass reacts more aggressively to resonance.
    static constexpr double bandpassResonanceBoost = 1.25;
    // OPEN: the manual's "36 dB/oct bandpass". Approximated with the 4-pole ladder's
    // bandpass tap (12 dB/oct each side) plus this much of the 2-pole lowpass tap.
    static constexpr double bandpassLowpassBlend = 0.15;
    // Saturation headroom: signals well below this pass nearly clean.
    static constexpr double headroom = 2.0;

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        update();
    }

    void setParameters (double cutoffHz, double resonance, Mode newMode)
    {
        cutoff = cutoffHz;
        res = std::clamp (resonance, 0.0, 1.0);
        mode = newMode;
        update();
    }

    void reset() { s1 = s2 = s3 = s4 = 0.0; }

    float process (float input)
    {
        // Linear prediction of the ladder output from the stage states.
        const double S = G * G * G * beta * s1 + G * G * beta * s2 + G * beta * s3 + beta * s4;

        // Gain compensation keeps the passband up as resonance rises (and drives the tanh harder).
        const double x = input * (1.0 + 0.5 * k);
        double u = (x - k * S) / (1.0 + k * G4);
        u = headroom * fastTanh (u / headroom);

        const double y1 = stage (u, s1);
        const double y2 = stage (y1, s2);
        const double y3 = stage (y2, s3);
        const double y4 = stage (y3, s4);

        if (mode == Mode::lowpass)
            return static_cast<float> (y4);

        // 4·LP²·HP²: a 2-pole lowpass times a 2-pole highpass, unity gain at the cutoff.
        const double bp = 4.0 * y2 - 8.0 * y3 + 4.0 * y4;
        return static_cast<float> (bp + bandpassLowpassBlend * y2);
    }

    double getCutoff() const { return cutoff; }

private:
    double stage (double in, double& s) const
    {
        const double v = (in - s) * G;
        const double y = v + s;
        s = y + v;
        return y;
    }

    void update()
    {
        const double fc = std::clamp (cutoff, 5.0, 0.45 * sampleRate);
        const double g = std::tan (3.141592653589793 * fc / sampleRate);
        G = g / (1.0 + g);
        beta = 1.0 / (1.0 + g);
        G4 = G * G * G * G;
        k = res * maxFeedback * (mode == Mode::bandpass ? bandpassResonanceBoost : 1.0);
    }

    double sampleRate = 44100.0;
    double cutoff = 1000.0;
    double res = 0.0;
    Mode mode = Mode::lowpass;
    double G = 0.0, beta = 1.0, G4 = 0.0, k = 0.0;
    double s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
};

// Cutoff after key tracking. amount 0..1 (1 = the cutoff follows pitch 1:1, so a
// self-oscillating filter plays in tune). Reference: cutoff knob = cutoff at middle C.
// OPEN: key-tracking range and reference note.
inline double keyTrackedCutoff (double cutoffHz, double pitchSemitones, double amount)
{
    constexpr double referenceNote = 60.0;
    return cutoffHz * std::pow (2.0, amount * (pitchSemitones - referenceNote) / 12.0);
}

} // namespace rotor
