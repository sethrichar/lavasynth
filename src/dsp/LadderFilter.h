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

    // Voicing lab — pick by ear, then lock one in. Names are descriptive; the inspirations are noted.
    enum class Character
    {
        rotor = 0,        // v1.4 default: 4-pole ladder, saturation at the input, partial bass compensation
        transistorLadder, // inspired by the Moog ladder: saturation in every stage, bass thins as resonance rises
        otaCascade,       // inspired by the CEM3320 (Prophet-5): 4-pole, cleaner, most of the bass kept
        stateVariable,    // inspired by the Oberheim SEM: 12 dB state-variable, soft and round
        screaming12       // inspired by the Korg MS-20: 12 dB, hard asymmetric clipping in the resonance path
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

    void setParameters (double cutoffHz, double resonance, Mode newMode, Character newCharacter = Character::rotor)
    {
        cutoff = cutoffHz;
        res = std::clamp (resonance, 0.0, 1.0);
        mode = newMode;
        if (newCharacter != character)
        {
            character = newCharacter;
            reset();
        }
        update();
    }

    void reset() { s1 = s2 = s3 = s4 = 0.0; }

    float process (float input)
    {
        switch (character)
        {
            case Character::rotor: return processRotor (input);
            case Character::transistorLadder: return (float) ((1.0 + 0.3 * k) * processLadder (input, true, 0.0)); // partial makeup; still thins
            case Character::otaCascade: return processLadder (input, false, 0.7);
            case Character::stateVariable: return (float) (svfTrim() * processStateVariable (input, false));
            case Character::screaming12: return (float) (svfTrim() * processStateVariable (input, true));
        }
        return 0.0f;
    }

    double getCutoff() const { return cutoff; }
    Character getCharacter() const { return character; }

    // Level-matching trim for the 12 dB voicings, whose resonant peak is broader and louder.
    double svfTrim() const { return 1.0 / (1.0 + 0.35 * res); }

private:
    float processRotor (float input)
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
        return static_cast<float> (bandpassLevel() * bp + bandpassLowpassBlend * y2);
    }

    // Tames the ladder bandpass's resonant peak so it sits ~6 dB above the lowpass at
    // self-oscillation instead of ~16 dB (still hotter: "reacts more aggressively").
    double bandpassLevel() const { return 1.0 / (1.0 + 0.4 * k); }

    // 4-pole ladder with saturation per stage (transistor ladder) or a lighter touch (OTA cascade).
    // bassCompensation: 0 = the classic ladder's bass loss at high resonance, 1 = fully compensated.
    float processLadder (float input, bool saturateStages, double bassCompensation)
    {
        const double S = G * G * G * beta * s1 + G * G * beta * s2 + G * beta * s3 + beta * s4;
        const double x = input * (1.0 + bassCompensation * k);
        double u = (x - k * S) / (1.0 + k * G4);
        u = headroom * fastTanh (u / headroom);

        const double drive = saturateStages ? 1.0 : 0.35;
        auto satStage = [&] (double in, double& st)
        {
            const double v = (stageSaturation * fastTanh (drive * in / stageSaturation) / drive - st) * G;
            const double y = v + st;
            st = y + v;
            return y;
        };
        const double y1 = satStage (u, s1);
        const double y2 = satStage (y1, s2);
        const double y3 = satStage (y2, s3);
        const double y4 = satStage (y3, s4);

        if (mode == Mode::lowpass)
            return static_cast<float> (y4);
        return static_cast<float> (bandpassLevel() * (4.0 * y2 - 8.0 * y3 + 4.0 * y4) + bandpassLowpassBlend * y2);
    }

    // 12 dB TPT state-variable filter. `screaming` adds hot input drive and hard asymmetric
    // clipping on the resonant (bandpass) state; otherwise a soft, symmetric limit.
    float processStateVariable (float input, bool screaming)
    {
        const double g = svfG;
        // Amplitude-dependent damping (Van der Pol style): the louder the resonant state, the more
        // damping, so self-oscillation settles at the same level at every pitch. Screaming is
        // asymmetric (settles higher on one side) for its ragged edge.
        const double c = screaming ? (s1 > 0.0 ? 0.04 : 0.09) : 0.02;
        const double R = svfDamping + c * s1 * s1;
        const double x = screaming ? inputDrive * input : 2.0 * fastTanh (0.5 * input); // SEM: soft input

        const double hp = (x - (2.0 * R + g) * s1 - s2) / (1.0 + 2.0 * R * g + g * g);
        const double v1 = g * hp;
        const double bp = v1 + s1;
        s1 = bp + v1;
        const double v2 = g * bp;
        const double lp = v2 + s2;
        s2 = lp + v2;

        const double outGain = screaming ? 1.0 / inputDrive : 1.0;
        if (mode == Mode::lowpass)
            return static_cast<float> (lp * outGain);
        // Bandpass ×2: unity at the cutoff at low resonance (the raw peak is 1 / 2R), rising with
        // resonance to ~6 dB above the lowpass at self-oscillation, like the ladder voicings.
        return static_cast<float> ((2.0 * bp + bandpassLowpassBlend * lp) * outGain);
    }

    static constexpr double inputDrive = 1.6; // screaming: hotter into the resonance

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

        // State-variable voicings: damping from 1 (Q 0.5) down to slightly negative at full
        // resonance so it self-oscillates; the state limiter holds the level.
        svfG = g;
        const double minDamping = character == Character::screaming12 ? -0.08 : -0.02;
        svfDamping = 1.0 - res * (1.0 - minDamping);
    }

    double sampleRate = 44100.0;
    double cutoff = 1000.0;
    double res = 0.0;
    Mode mode = Mode::lowpass;
    Character character = Character::rotor;
    static constexpr double stageSaturation = 1.5;
    double svfG = 0.0, svfDamping = 1.0;
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
