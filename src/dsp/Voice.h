#pragma once

#include "Envelope.h"
#include "Glide.h"
#include "LadderFilter.h"
#include "Oscillator.h"
#include "Shapers.h"

#include <cmath>

namespace rotor
{

// One of the five fixed voices. Runs at the oversampled rate (see Decimator4x):
//   [main osc + sub osc + noise] → OSC-level overdrive → ladder filter → wavefolder
//   → amp envelope → voice level
class Voice
{
public:
    enum class SubWaveform
    {
        sine = 0,
        square
    };

    struct Parameters
    {
        // Per-voice panel controls
        Waveform waveform = Waveform::sharkTooth;
        int octave = 0;              // -2..+2
        double level = 1.0;          // 0..1, after the amp

        // Global section
        double oscLevel = 0.5;       // 0..1; above 0.5 overdrives
        double subLevel = 0.0;       // 0..1
        int subOctave = 1;           // 1 or 2 octaves below the voice
        SubWaveform subWaveform = SubWaveform::sine;
        double noiseLevel = 0.0;     // 0..1 (noise itself comes in pre-coloured)
        double glideSecondsPerOctave = 0.0;

        // Filter
        LadderFilter::Mode filterMode = LadderFilter::Mode::lowpass;
        double cutoffHz = 8000.0;
        double resonance = 0.1;      // 0..1
        double keyTrack = 0.0;       // 0..1

        // Wavefolder
        double fold = 0.0;           // 0..1

        Envelope::Parameters amp;
    };

    static double midiNoteToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    // sampleRate here is the rate the voice runs at (the oversampled rate in the plugin).
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        osc.setSampleRate (sampleRate);
        sub.setSampleRate (sampleRate);
        filter.setSampleRate (sampleRate);
        env.setSampleRate (sampleRate);
        glide.setSampleRate (sampleRate);
        levelCoef = 1.0 - std::exp (-1.0 / (levelSmoothingSeconds * sampleRate));
        reset();
    }

    void reset()
    {
        osc.reset();
        sub.reset();
        filter.reset();
        env.reset();
        glide.reset();
        currentNote = -1;
        smoothedLevel = params.level;
    }

    void setParameters (const Parameters& p)
    {
        const bool pitchChanged = p.octave != params.octave || p.subOctave != params.subOctave;
        params = p;
        osc.setWaveform (p.waveform);
        sub.setWaveform (p.subWaveform == SubWaveform::square ? Waveform::square : Waveform::sine);
        env.setParameters (p.amp);
        glide.setTimePerOctave (p.glideSecondsPerOctave);
        if (pitchChanged)
            updateFrequency();
        updateFilter();
    }

    void noteOn (int midiNote, float newVelocity)
    {
        if (! env.isActive())
        {
            osc.reset();
            sub.reset();
        }
        currentNote = midiNote;
        velocity = newVelocity;
        // Each voice glides from its own previous note (confirmed by owner).
        glide.setTarget (midiNote);
        updateFrequency();
        updateFilter();
        env.noteOn();
    }

    // Unison redistribution: change pitch (gliding if enabled) without retriggering the envelopes.
    void moveTo (int midiNote)
    {
        currentNote = midiNote;
        glide.setTarget (midiNote);
        updateFrequency();
        updateFilter();
    }

    void noteOff() { env.noteOff(); }

    bool isActive() const { return env.isActive(); }
    int getCurrentNote() const { return currentNote; }
    double getCurrentPitch() const { return glide.getCurrent() + 12.0 * params.octave; }
    double getFilterCutoff() const { return filter.getCutoff(); }

    // noise: this sample of the shared (tilt-EQ'd) noise source.
    float process (float noise = 0.0f)
    {
        if (! env.isActive())
            return 0.0f;

        if (glide.isGliding())
        {
            glide.process();
            updateFrequency();
        }

        // OPEN: how a voice's level couples into its sub/filter/LFO when the main osc is at 0.
        // Here the voice level scales the whole voice after the amp, so the sub and noise follow it.
        const double mix = OscLevel::mainGain (params.oscLevel) * osc.process()
                           + params.subLevel * sub.process()
                           + params.noiseLevel * noise;

        float s = oscOverdrive ((float) mix, OscLevel::drive (params.oscLevel));
        s = filter.process (s);
        s = wavefold (s, params.fold);

        smoothedLevel += (params.level - smoothedLevel) * levelCoef;
        return s * env.process() * velocity * (float) smoothedLevel;
    }

private:
    void updateFrequency()
    {
        if (currentNote < 0)
            return;
        const double hz = midiNoteToHz (getCurrentPitch());
        osc.setFrequency (hz);
        sub.setFrequency (hz / (params.subOctave == 2 ? 4.0 : 2.0));
    }

    void updateFilter()
    {
        const double pitch = currentNote >= 0 ? getCurrentPitch() : 60.0;
        filter.setParameters (keyTrackedCutoff (params.cutoffHz, pitch, params.keyTrack), params.resonance, params.filterMode);
    }

    static constexpr double levelSmoothingSeconds = 0.01;

    double sampleRate = 44100.0;
    Parameters params;
    double smoothedLevel = 1.0;
    double levelCoef = 1.0;
    Oscillator osc;
    Oscillator sub;
    LadderFilter filter;
    Envelope env;
    Glide glide;
    int currentNote = -1;
    float velocity = 1.0f;
};

} // namespace rotor
