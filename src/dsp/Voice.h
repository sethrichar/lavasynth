#pragma once

#include "Envelope.h"
#include "Glide.h"
#include "LowpassFilter.h"
#include "Oscillator.h"

#include <cmath>

namespace rotor
{

// One of the five fixed voices: oscillator → lowpass → amp envelope → voice level.
class Voice
{
public:
    struct Parameters
    {
        // Per-voice panel controls
        Waveform waveform = Waveform::sharkTooth;
        int octave = 0;              // -2..+2
        double level = 1.0;          // 0..1, after the amp

        // Shared controls
        double cutoffHz = 8000.0;
        double resonance = 0.1;      // 0..1
        double glideSecondsPerOctave = 0.0;
        Envelope::Parameters amp;
    };

    static double midiNoteToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        osc.setSampleRate (sampleRate);
        filter.setSampleRate (sampleRate);
        env.setSampleRate (sampleRate);
        glide.setSampleRate (sampleRate);
        levelCoef = 1.0 - std::exp (-1.0 / (levelSmoothingSeconds * sampleRate));
        reset();
    }

    void reset()
    {
        osc.reset();
        filter.reset();
        env.reset();
        glide.reset();
        currentNote = -1;
        smoothedLevel = params.level;
    }

    void setParameters (const Parameters& p)
    {
        const bool octaveChanged = p.octave != params.octave;
        params = p;
        osc.setWaveform (p.waveform);
        filter.setParameters (p.cutoffHz, p.resonance);
        env.setParameters (p.amp);
        glide.setTimePerOctave (p.glideSecondsPerOctave);
        if (octaveChanged)
            updateFrequency();
    }

    void noteOn (int midiNote, float newVelocity)
    {
        if (! env.isActive())
            osc.reset();
        currentNote = midiNote;
        velocity = newVelocity;
        // Each voice glides from its own previous note (confirmed by owner).
        glide.setTarget (midiNote);
        updateFrequency();
        env.noteOn();
    }

    // Unison redistribution: change pitch (gliding if enabled) without retriggering the envelopes.
    void moveTo (int midiNote)
    {
        currentNote = midiNote;
        glide.setTarget (midiNote);
        updateFrequency();
    }

    void noteOff() { env.noteOff(); }

    bool isActive() const { return env.isActive(); }
    int getCurrentNote() const { return currentNote; }
    double getCurrentPitch() const { return glide.getCurrent() + 12.0 * params.octave; }

    float process()
    {
        if (! env.isActive())
            return 0.0f;

        if (glide.isGliding())
        {
            glide.process();
            updateFrequency();
        }

        smoothedLevel += (params.level - smoothedLevel) * levelCoef;
        const float s = filter.process (osc.process());
        return s * env.process() * velocity * (float) smoothedLevel;
    }

private:
    void updateFrequency()
    {
        if (currentNote >= 0)
            osc.setFrequency (midiNoteToHz (getCurrentPitch()));
    }

    static constexpr double levelSmoothingSeconds = 0.01;

    double sampleRate = 44100.0;
    Parameters params;
    double smoothedLevel = 1.0;
    double levelCoef = 1.0;
    Oscillator osc;
    LowpassFilter filter;
    Envelope env;
    Glide glide;
    int currentNote = -1;
    float velocity = 1.0f;
};

} // namespace rotor
