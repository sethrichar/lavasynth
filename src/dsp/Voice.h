#pragma once

#include "ControlFold.h"
#include "Envelope.h"
#include "Expression.h"
#include "PerformanceTuning.h"
#include "Glide.h"
#include "LadderFilter.h"
#include "Lfo.h"
#include "Oscillator.h"
#include "Shapers.h"
#include "Wildcards.h"

#include <cmath>

namespace rotor
{

// One of the five fixed voices. Runs at the oversampled rate (see Decimator4x):
//   [main osc + sub osc + noise] → OSC-level overdrive → ladder filter → wavefolder
//   → amp envelope → voice level
// The mod envelope and LFO (one each per voice, shared controls) modulate phase distortion,
// cutoff, wavefolder and LFO rate here; the spreader (v1.6) reads getModEnvLevel()/getLfoValue().
// Phase distortion, spreader and wavefolder fold back at the ends of their ranges.
class Voice
{
public:
    // OPEN: modulation depth ranges.
    static constexpr double modEnvCutoffOctaves = 5.0;  // mod env → cutoff at full depth
    static constexpr double modEnvLfoRateOctaves = 4.0; // mod env → LFO rate at full depth
    static constexpr double lfoCutoffOctaves = 4.0;     // LFO → cutoff at full depth (each way)

    struct LfoSettings
    {
        Lfo::Shape shape = Lfo::Shape::sine;
        double rateHz = 1.0;         // after range and host sync, before key tracking
        double keyTrack = 0.0;       // -1..+1
        bool retrigger = false;      // restart on every key press
    };

    // Bipolar depths, -1..+1 (the spreader depth is used in v1.6).
    struct LfoDepths
    {
        double phaseDistortion = 0.0;
        double cutoff = 0.0;
        double spread = 0.0;
        double fold = 0.0;
    };

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
        LadderFilter::Character filterCharacter = LadderFilter::Character::rotor;
        double cutoffHz = 8000.0;
        double resonance = 0.1;      // 0..1
        double keyTrack = 0.0;       // 0..1

        // Wavefolder
        double fold = 0.0;           // 0..1

        // Phase distortion on the main oscillator, -1..+1.
        // OPEN: the hardware may have no base PD control (modulation only); this is the offset.
        double phaseDistortion = 0.0;

        Envelope::Parameters amp;
        Envelope::Parameters mod;
        double ampKeyTrack = 0.0;    // -1..+1
        double modKeyTrack = 0.0;    // -1..+1
        ModEnvDepths modEnvDepth;
        LfoSettings lfo;
        LfoDepths lfoDepth;

        // Wildcards (0..1 each). The wavefolder wildcard is `fold` above.
        Wildcards::Amounts wild;
        double spread = 0.0;         // stereo spreader width, 0..1

        // Aftertouch sliders (bipolar), mod wheel and tuning.
        double atWildcard = 0.0;     // up: pressure blends in the pitch wildcards; down: harmonic clusters
        double atCutoff = 0.0;
        double atLfoRate = 0.0;
        double wheelWildcardBlend = 0.0; // mod wheel in "Wildcards" mode (0..1)
        double wheelPitchLfo = 0.0;      // mod wheel in "Pitch LFO" mode (0..1)
        double globalDetune = 0.0;       // -1..+1 → ± performance::globalDetuneMaxSemitones
        double pitchDrift = 0.0;         // 0..1
    };

    // Per-note expression (pitch bend, pressure, MPE timbre). Set before setParameters().
    void setExpression (const Expression& e) { expression = e; }

    // Voice number (0-based, sets its spreader position) and its own random sequences.
    void setIdentity (int index, std::uint32_t seed)
    {
        voiceIndex = index;
        lfo = Lfo (seed);
        lfo.setSampleRate (sampleRate);
        wildcards = Wildcards (seed ^ 0xA5A5A5A5u);
        Random r (seed ^ 0x5EEDu);
        driftError = r.bipolar(); // this voice's own tracking error (Pitch Drift)
    }

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
        lfo.setSampleRate (sampleRate);
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
        // Aftertouch up (and the mod wheel in Wildcards mode) blend in the four pitch wildcards.
        auto amounts = p.wild;
        const double blend = std::max (0.0, p.atWildcard) * expression.pressure + p.wheelWildcardBlend;
        amounts.noteDetune = std::min (1.0, amounts.noteDetune + blend);
        amounts.wow = std::min (1.0, amounts.wow + blend);
        amounts.flutter = std::min (1.0, amounts.flutter + blend);
        amounts.reelDrag = std::min (1.0, amounts.reelDrag + blend);
        wildcards.setAmounts (amounts);
        applyAmpEnvelope();
        modEnv.setParameters (p.mod);
        lfo.setShape (p.lfo.shape);
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

        // Wildcards drawn per trigger: note detune and envelope scatter.
        wildcards.trigger();
        scatter = wildcards.drawScatter (params.amp.attackSeconds <= Envelope::minTimeSeconds * 1.001,
                                         params.amp.decaySeconds <= Envelope::minTimeSeconds * 1.001,
                                         params.amp.releaseSeconds >= maxReleaseSeconds * 0.999);
        applyAmpEnvelope();

        updateFrequency();
        updateEnvelopeRates();
        env.noteOn();
        modEnv.noteOn();
        if (params.lfo.retrigger)
            lfo.reset();
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
    double getPhaseDistortion() const { return phaseDistortionAmount; }
    float getLfoValue() const { return lfo.getValue(); }
    double getLfoFrequency() const { return lfo.getFrequency(); }
    double getPan() const { return pan; }
    float getPanLeft() const { return panLeft; }
    float getPanRight() const { return panRight; }
    double getPitchOffsetCents() const { return wildcards.getPitchCents(); }
    // Everything added to the played pitch (semitones): wildcards, bend, detune, drift, clusters, vibrato.
    double getPitchOffsetSemitones() const { return pitchOffsetSemitones(); }
    Wildcards::Scatter getScatter() const { return scatter; }
    static constexpr double maxReleaseSeconds = 3600.0;

    // noise: this sample of the shared (tilt-EQ'd) noise source.
    float process (float noise = 0.0f)
    {
        lfo.process(); // free-running even while the voice is silent

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
        double mix = OscLevel::mainGain (params.oscLevel) * osc.process()
                     + params.subLevel * sub.process()
                     + params.noiseLevel * noise;
        if (noiseBurstLevel > 0.0) // chaos wildcard
            mix += noiseBurstLevel * wildcards.noiseSample();

        float s = oscOverdrive ((float) mix, OscLevel::drive (params.oscLevel));
        s = filter.process (s);
        s = wavefold (s, foldAmount);

        smoothedLevel += (params.level - smoothedLevel) * levelCoef;
        return s * env.process() * velocity * (float) (smoothedLevel * wildcardGain);
    }

private:
    void updateFrequency()
    {
        if (currentNote < 0)
            return;
        const double hz = midiNoteToHz (getCurrentPitch() + pitchOffsetSemitones());
        osc.setFrequency (hz);
        sub.setFrequency (hz / (params.subOctave == 2 ? 4.0 : 2.0));
    }

    double pitchOffsetSemitones() const
    {
        namespace perf = performance;
        const double pitch = getCurrentPitch();
        double offset = 0.01 * wildcards.getPitchCents() + expression.bendSemitones
                        + params.globalDetune * perf::globalDetuneMaxSemitones;

        // Pitch Drift: the further from this voice's centre note, the further out of tune.
        offset += params.pitchDrift * 0.01 * perf::driftMaxCentsPerOctave * driftError
                  * (pitch - perf::driftCentreNotes[voiceIndex]) / 12.0;

        // Wildcard AT down: harmonic clusters (voice 3 stays put).
        if (params.atWildcard < 0.0)
            offset += -params.atWildcard * expression.pressure * perf::clusterSemitones[voiceIndex];

        // Mod wheel in Pitch LFO mode: vibrato from this voice's LFO.
        offset += params.wheelPitchLfo * perf::pitchLfoMaxSemitones * lfo.getValue();
        return offset;
    }

    double pitchForTracking() const { return currentNote >= 0 ? getCurrentPitch() : 60.0; }

    void updateEnvelopeRates()
    {
        const double pitch = pitchForTracking();
        env.setRateScale (keyTrackedRate (pitch, params.ampKeyTrack));
        modEnv.setRateScale (keyTrackedRate (pitch, params.modKeyTrack));
    }

    // Mod envelope + LFO destinations, refreshed every few samples.
    void updateModulation()
    {
        modulationCountdown = modulationInterval;

        wildcards.tick (modulationInterval / sampleRate);
        updateFrequency(); // pitch wildcards
        wildcardGain = wildcards.getGain();
        noiseBurstLevel = wildcards.getNoiseBurstLevel();

        const double m = modEnv.getLevel();
        const double l = lfo.getValue();
        const double pitch = pitchForTracking();
        const auto& envDepth = params.modEnvDepth;
        const auto& lfoDepth = params.lfoDepth;

        lfo.setFrequency (params.lfo.rateHz * keyTrackedRate (pitch, params.lfo.keyTrack)
                          * std::exp2 (envDepth.lfoRate * m * modEnvLfoRateOctaves
                                       + params.atLfoRate * expression.pressure * performance::atLfoRateOctaves));

        const double cutoff = keyTrackedCutoff (params.cutoffHz, pitch, params.keyTrack)
                              * std::exp2 (envDepth.cutoff * m * modEnvCutoffOctaves + lfoDepth.cutoff * l * lfoCutoffOctaves
                                           + wildcards.getCutoffOctaves()
                                           + params.atCutoff * expression.pressure * performance::atCutoffOctaves
                                           + (expression.timbre - 0.5) * 2.0 * performance::timbreCutoffOctaves);
        filter.setParameters (cutoff, params.resonance, params.filterMode, params.filterCharacter);

        // Control-signal wavefolding: past the ends of the range these reflect back.
        foldAmount = foldIntoRange (params.fold + envDepth.fold * m + lfoDepth.fold * l, 0.0, 1.0);
        phaseDistortionAmount = foldIntoRange (params.phaseDistortion + envDepth.phaseDistortion * m
                                                   + lfoDepth.phaseDistortion * l, -1.0, 1.0);
        osc.setPhaseDistortion (phaseDistortionAmount);

        pan = spreadPan (voiceIndex, params.spread, envDepth.spread * m, lfoDepth.spread * l);
        panGains (pan, panLeft, panRight);
    }

    void applyAmpEnvelope()
    {
        auto amp = params.amp;
        amp.attackSeconds *= scatter.attack;
        amp.decaySeconds *= scatter.decay;
        amp.releaseSeconds = std::min (amp.releaseSeconds * scatter.release, maxReleaseSeconds);
        env.setParameters (amp);
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
    Lfo lfo;
    Wildcards wildcards;
    Wildcards::Scatter scatter;
    Expression expression;
    double driftError = 0.0;
    int voiceIndex = 0;
    double wildcardGain = 1.0;
    double noiseBurstLevel = 0.0;
    double pan = 0.0;
    float panLeft = 0.70710678f, panRight = 0.70710678f;
    double foldAmount = 0.0;
    double phaseDistortionAmount = 0.0;
    int modulationCountdown = 0;
    int currentNote = -1;
    float velocity = 1.0f;
};

} // namespace rotor
