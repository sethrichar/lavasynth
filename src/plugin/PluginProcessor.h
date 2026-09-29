#pragma once

#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/Drive.h"
#include "dsp/Expression.h"
#include "dsp/HostSync.h"
#include "dsp/LfoMinMax.h"
#include "dsp/Reverb.h"
#include "dsp/Noise.h"
#include "dsp/Oversampling.h"
#include "dsp/Voice.h"
#include "dsp/VoiceAllocator.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

class RotorAudioProcessor final : public juce::AudioProcessor
{
public:
    RotorAudioProcessor();
    ~RotorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Host programs = the factory presets.
    int getNumPrograms() override { return presetManager.getNumFactoryPresets(); }
    int getCurrentProgram() override { return juce::jmin (presetManager.getCurrentIndex(), getNumPrograms() - 1); }
    void setCurrentProgram (int index) override { presetManager.loadPreset (index); }
    const juce::String getProgramName (int index) override
    {
        const auto& f = PresetManager::factoryPresets();
        return index >= 0 && index < (int) f.size() ? juce::String (f[(size_t) index].name) : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() { return state; }
    PresetManager& getPresetManager() { return presetManager; }

private:
    void handleMidi (const juce::MidiMessage& message);
    void apply (const rotor::VoiceAllocator::Result& result);
    void updateVoiceParameters();
    void render (float* left, float* right, int numSamples, int offset);

    juce::AudioProcessorValueTreeState state;
    PresetManager presetManager { state };

    struct VoiceParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* octave = nullptr;
        std::atomic<float>* waveform = nullptr;
    };
    std::array<VoiceParams, rotor::numVoices> voiceParams;

    std::atomic<float>* voiceModeParam = nullptr;
    std::atomic<float>* roundRobinResetParam = nullptr;
    std::atomic<float>* unisonGraceParam = nullptr;
    std::atomic<float>* monoPriorityParam = nullptr;
    std::atomic<float>* oscLevelParam = nullptr;
    std::atomic<float>* subLevelParam = nullptr;
    std::atomic<float>* subOctaveParam = nullptr;
    std::atomic<float>* subWaveformParam = nullptr;
    std::atomic<float>* noiseLevelParam = nullptr;
    std::atomic<float>* noiseColorParam = nullptr;
    std::atomic<float>* glideParam = nullptr;
    std::atomic<float>* filterModeParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* keyTrackParam = nullptr;
    std::atomic<float>* foldParam = nullptr;
    struct EnvelopeParams
    {
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* loop = nullptr;
        std::atomic<float>* keyTrack = nullptr;
    };
    EnvelopeParams ampEnvParams, modEnvParams;
    rotor::Envelope::Parameters readEnvelope (const EnvelopeParams& e) const;

    std::atomic<float>* modEnvToPdParam = nullptr;
    std::atomic<float>* modEnvToCutoffParam = nullptr;
    std::atomic<float>* modEnvToLfoRateParam = nullptr;
    std::atomic<float>* modEnvToSpreadParam = nullptr;
    std::atomic<float>* modEnvToFoldParam = nullptr;
    std::atomic<float>* envSyncParam = nullptr;
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoRangeParam = nullptr;
    std::atomic<float>* lfoShapeParam = nullptr;
    std::atomic<float>* lfoKeyTrackParam = nullptr;
    std::atomic<float>* lfoRetriggerParam = nullptr;
    std::atomic<float>* lfoSyncParam = nullptr;
    std::atomic<float>* lfoToPdParam = nullptr;
    std::atomic<float>* lfoToCutoffParam = nullptr;
    std::atomic<float>* lfoToSpreadParam = nullptr;
    std::atomic<float>* lfoToFoldParam = nullptr;
    std::atomic<float>* phaseDistParam = nullptr;
    std::atomic<float>* noteDetuneParam = nullptr;
    std::atomic<float>* wowParam = nullptr;
    std::atomic<float>* flutterParam = nullptr;
    std::atomic<float>* reelDragParam = nullptr;
    std::atomic<float>* chaosParam = nullptr;
    std::atomic<float>* envScatterParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* envCurveParam = nullptr;
    std::atomic<float>* reverbAmountParam = nullptr;
    std::atomic<float>* reverbMixParam = nullptr;
    std::atomic<float>* driveAmountParam = nullptr;
    std::atomic<float>* driveMixParam = nullptr;
    std::atomic<float>* fxColorParam = nullptr;
    std::atomic<float>* lfoToReverbMixParam = nullptr;
    std::atomic<float>* lfoToDriveMixParam = nullptr;
    std::atomic<float>* lfoToFxAmountParam = nullptr;
    std::atomic<float>* lfoToFxColorParam = nullptr;
    std::atomic<float>* atWildcardParam = nullptr;
    std::atomic<float>* atCutoffParam = nullptr;
    std::atomic<float>* atLfoRateParam = nullptr;
    std::atomic<float>* mpeParam = nullptr;
    std::atomic<float>* modWheelModeParam = nullptr;
    std::atomic<float>* tuneModeParam = nullptr;
    std::atomic<float>* tuneParam = nullptr;
    std::atomic<float>* extInputParam = nullptr;

    // Performance expression: per-channel bend/pressure/timbre; which channel each voice's note came from.
    rotor::MidiExpression expression;
    std::array<int, 128> noteChannel {};
    std::array<int, rotor::numVoices> voiceChannel {};

    // External (sidechain) input, copied out before the output buffer is cleared.
    juce::AudioBuffer<float> extBuffer;
    int extChannels = 0;
    rotor::TiltEq extTiltLeft, extTiltRight;
    float previousExt[3] {}; // mono, left, right
    void updateEffects();

    rotor::PlateReverb reverb;
    rotor::CmosDrive driveLeft, driveRight;
    std::atomic<float>* filterCharacterParam = nullptr;
    double hostBpm = 120.0;
    std::atomic<float>* levelParam = nullptr;

    rotor::VoiceAllocator allocator;
    std::array<rotor::Voice, rotor::numVoices> voices;
    float lastVelocity = 1.0f;

    // Shared noise source (EXT input will replace it in v1.8) and its Color tilt EQ.
    rotor::PinkNoise noise;
    rotor::TiltEq noiseTilt;

    // Voices run 4× oversampled; each is panned, summed, then decimated back down per channel.
    rotor::Decimator4x decimatorLeft, decimatorRight;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float> smoothedResonance;
    juce::SmoothedValue<float> smoothedOscLevel;
    juce::SmoothedValue<float> smoothedSubLevel;
    juce::SmoothedValue<float> smoothedNoiseLevel;
    juce::SmoothedValue<float> smoothedNoiseColor;
    juce::SmoothedValue<float> smoothedFold;
    juce::SmoothedValue<float> smoothedModEnvToCutoff;
    juce::SmoothedValue<float> smoothedModEnvToFold;
    juce::SmoothedValue<float> smoothedLfoRate;
    juce::SmoothedValue<float> smoothedLfoToPd;
    juce::SmoothedValue<float> smoothedLfoToCutoff;
    juce::SmoothedValue<float> smoothedLfoToFold;
    juce::SmoothedValue<float> smoothedModEnvToPd;
    juce::SmoothedValue<float> smoothedPhaseDist;
    juce::SmoothedValue<float> smoothedSpread;
    juce::SmoothedValue<float> smoothedReverbAmount, smoothedReverbMix, smoothedDriveAmount, smoothedDriveMix, smoothedFxColor;
    juce::SmoothedValue<float> smoothedLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotorAudioProcessor)
};
