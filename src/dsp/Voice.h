#pragma once

#include "Envelope.h"
#include "LowpassFilter.h"
#include "Oscillator.h"

#include <cmath>

namespace rotor
{

// One synth voice (v1.1): oscillator → lowpass → amp envelope.
// v1.2 turns this into five fixed voices with level/octave/on-off.
class Voice
{
public:
    struct Parameters
    {
        Waveform waveform = Waveform::sharkTooth;
        int octave = 0;              // -2..+2
        double cutoffHz = 8000.0;
        double resonance = 0.1;      // 0..1
        Envelope::Parameters amp;
    };

    static double midiNoteToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        osc.setSampleRate (sampleRate);
        filter.setSampleRate (sampleRate);
        env.setSampleRate (sampleRate);
        reset();
    }

    void reset()
    {
        osc.reset();
        filter.reset();
        env.reset();
        currentNote = -1;
    }

    void setParameters (const Parameters& p)
    {
        params = p;
        osc.setWaveform (p.waveform);
        filter.setParameters (p.cutoffHz, p.resonance);
        env.setParameters (p.amp);
        updatePitch();
    }

    void noteOn (int midiNote, float newVelocity)
    {
        if (! env.isActive())
            osc.reset();
        currentNote = midiNote;
        velocity = newVelocity;
        updatePitch();
        env.noteOn();
    }

    void noteOff() { env.noteOff(); }

    bool isActive() const { return env.isActive(); }
    int getCurrentNote() const { return currentNote; }

    float process()
    {
        if (! env.isActive())
            return 0.0f;

        const float s = filter.process (osc.process());
        return s * env.process() * velocity;
    }

private:
    void updatePitch()
    {
        if (currentNote >= 0)
            osc.setFrequency (midiNoteToHz (currentNote + 12.0 * params.octave));
    }

    double sampleRate = 44100.0;
    Parameters params;
    Oscillator osc;
    LowpassFilter filter;
    Envelope env;
    int currentNote = -1;
    float velocity = 1.0f;
};

} // namespace rotor
