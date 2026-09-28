#pragma once

#include "ControlFold.h"
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
// The mod envelope (one per voice, shared controls) modulates cutoff and wavefolder here;
// its other destinations (phase distortion, LFO rate, spreader) read getModEnvLevel().
class Voice
{
public:
    // OPEN: mod-envelope depth ranges. Full depth moves the cutoff this many octaves.
    static constexpr double modEnvCutoffOctaves = 5.0;

    // Bipolar depths, -1..+1.
    struct ModEnvDepths
    {
        double phaseDistortion = 0.0;
        double cutoff = 0.0;
        double lfoRate = 0.0;
        double spread = 0.0;
        double fold = 0.0;
    };

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
        Envelope::Parameters mod;
        double ampKeyTrack = 0.0;    // -1..+1
        double modKeyTrack = 0.0;    // -1..+1
        ModEnvDepths modEnvDepth;
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
        modEnv.setSampleRate (sampleRate);
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
        modEnv.reset();
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
        modEnv.setParameters (p.mod);
        glide.setTimePerOctave (p.glideSecondsPerOctave);
        if (pitchChanged)
            updateFrequency();
        updateEnvelopeRates();
        updateModulation();
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
        updateEnvelopeRates();
        env.noteOn();
        modEnv.noteOn();
        updateModulation();
    }

    // Unison redistribution: change pitch (gliding if enabled) without retriggering the envelopes.
    void moveTo (int midiNote)
    {
        currentNote = midiNote;
        glide.setTarget (midiNote);
        updateFrequency();
        updateEnvelopeRates();
        updateModulation();
    }

    void noteOff()
    {
        env.noteOff();
        modEnv.noteOff();
    }

    bool isActive() const { return env.isActive(); }
    int getCurrentNote() const { return currentNote; }
    double getCurrentPitch() const { return glide.getCurrent() + 12.0 * params.octave; }
    double getFilterCutoff() const { return filter.getCutoff(); }
    double getModEnvLevel() const { return modEnv.getLevel(); }
    double getAmpEnvLevel() const { return env.getLevel(); }
    double getFoldAmount() const { return foldAmount; }

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

        modEnv.process();
        if (--modulationCountdown <= 0)
            updateModulation();

        // OPEN: how a voice's level couples into its sub/filter/LFO when the main osc is at 0.
        // Here the voice level scales the whole voice after the amp, so the sub and noise follow it.
        const double mix = OscLevel::mainGain (params.oscLevel) * osc.process()
                           + params.subLevel * sub.process()
                           + params.noiseLevel * noise;

        float s = oscOverdrive ((float) mix, OscLevel::drive (params.oscLevel));
        s = filter.process (s);
        s = wavefold (s, foldAmount);

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

    double pitchForTracking() const { return currentNote >= 0 ? getCurrentPitch() : 60.0; }

    void updateEnvelopeRates()
    {
        const double pitch = pitchForTracking();
        env.setRateScale (keyTrackedRate (pitch, params.ampKeyTrack));
        modEnv.setRateScale (keyTrackedRate (pitch, params.modKeyTrack));
    }

    // Cutoff and wavefolder follow the mod envelope, refreshed every few samples.
    void updateModulation()
    {
        modulationCountdown = modulationInterval;
        const double m = modEnv.getLevel();

        const double cutoff = keyTrackedCutoff (params.cutoffHz, pitchForTracking(), params.keyTrack)
                              * std::exp2 (params.modEnvDepth.cutoff * m * modEnvCutoffOctaves);
        filter.setParameters (cutoff, params.resonance, params.filterMode);

        // Control-signal wavefolding: past the ends of the range the amount reflects back.
        foldAmount = foldIntoRange (params.fold + params.modEnvDepth.fold * m, 0.0, 1.0);
    }

    static constexpr double levelSmoothingSeconds = 0.01;
    static constexpr int modulationInterval = 8; // samples at the voice rate

    double sampleRate = 44100.0;
    Parameters params;
    double smoothedLevel = 1.0;
    double levelCoef = 1.0;
    Oscillator osc;
    Oscillator sub;
    LadderFilter filter;
    Envelope env;
    Envelope modEnv;
    Glide glide;
    double foldAmount = 0.0;
    int modulationCountdown = 0;
    int currentNote = -1;
    float velocity = 1.0f;
};

} // namespace rotor
