#pragma once

#include "Parameters.h"
#include "dsp/Voice.h"

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
    void noteOn (int note, float velocity);
    void noteOff (int note);
    bool removeHeld (int note);
    void allNotesOff();
    void updateVoiceParameters();
    void renderVoice (float* left, float* right, int numSamples);

    juce::AudioProcessorValueTreeState state;

    std::atomic<float>* waveformParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* sustainParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* levelParam = nullptr;

    rotor::Voice voice;

    // Held keys, oldest first; last-note priority for the single v1.1 voice.
    std::array<int, 128> heldNotes {};
    int numHeld = 0;
    float lastVelocity = 1.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float> smoothedResonance;
    juce::SmoothedValue<float> smoothedLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotorAudioProcessor)
};
