#pragma once

#include "Parameters.h"
#include "dsp/HostSync.h"
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() { return state; }

private:
    void handleMidi (const juce::MidiMessage& message);
    void apply (const rotor::VoiceAllocator::Result& result);
    void updateVoiceParameters();
    void render (float* left, float* right, int numSamples);

    juce::AudioProcessorValueTreeState state;

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
    double hostBpm = 120.0;
    std::atomic<float>* levelParam = nullptr;

    rotor::VoiceAllocator allocator;
    std::array<rotor::Voice, rotor::numVoices> voices;
    float lastVelocity = 1.0f;

    // Shared noise source (EXT input will replace it in v1.8) and its Color tilt EQ.
    rotor::PinkNoise noise;
    rotor::TiltEq noiseTilt;
    float previousNoise = 0.0f;

    // Voices run 4× oversampled and are summed before decimating back down.
    rotor::Decimator4x decimator;

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
    juce::SmoothedValue<float> smoothedLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotorAudioProcessor)
};
