#include "PluginProcessor.h"

namespace
{
    // Parameters that are expensive to apply (filter coefficients) are updated every this many samples.
    constexpr int controlBlockSize = 32;
    constexpr double smoothingSeconds = 0.02;
} // namespace

RotorAudioProcessor::RotorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "RotorState", rotor::params::createLayout())
{
    using namespace rotor::params;
    waveformParam = state.getRawParameterValue (waveform);
    octaveParam = state.getRawParameterValue (octave);
    cutoffParam = state.getRawParameterValue (cutoff);
    resonanceParam = state.getRawParameterValue (resonance);
    attackParam = state.getRawParameterValue (attack);
    decayParam = state.getRawParameterValue (decay);
    sustainParam = state.getRawParameterValue (sustain);
    releaseParam = state.getRawParameterValue (release);
    levelParam = state.getRawParameterValue (level);
}

bool RotorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RotorAudioProcessor::prepareToPlay (double sampleRate, int)
{
    voice.prepare (sampleRate);
    numHeld = 0;

    // Control-rate smoothers tick once per control block, the level smoother once per sample.
    const double controlRate = sampleRate / controlBlockSize;
    smoothedCutoff.reset (controlRate, smoothingSeconds);
    smoothedResonance.reset (controlRate, smoothingSeconds);
    smoothedLevel.reset (sampleRate, smoothingSeconds);
    smoothedCutoff.setCurrentAndTargetValue (cutoffParam->load());
    smoothedResonance.setCurrentAndTargetValue (resonanceParam->load());
    smoothedLevel.setCurrentAndTargetValue (levelParam->load());

    updateVoiceParameters();
}

void RotorAudioProcessor::updateVoiceParameters()
{
    rotor::Voice::Parameters p;
    p.waveform = static_cast<rotor::Waveform> (juce::jlimit (0, rotor::numWaveforms - 1, (int) waveformParam->load()));
    p.octave = juce::jlimit (-2, 2, juce::roundToInt (octaveParam->load()));
    p.cutoffHz = smoothedCutoff.getNextValue();
    p.resonance = smoothedResonance.getNextValue();
    p.amp.attackSeconds = attackParam->load();
    p.amp.decaySeconds = decayParam->load();
    p.amp.sustainLevel = sustainParam->load();
    p.amp.releaseSeconds = releaseParam->load();
    voice.setParameters (p);
}

void RotorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    smoothedCutoff.setTargetValue (cutoffParam->load());
    smoothedResonance.setTargetValue (resonanceParam->load());
    smoothedLevel.setTargetValue (levelParam->load());

    const int numSamples = buffer.getNumSamples();
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto midiIt = midi.cbegin();
    int pos = 0;
    while (pos < numSamples)
    {
        // Split at control-block boundaries and at MIDI events for sample-accurate notes.
        int end = std::min (numSamples, pos + controlBlockSize);
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= pos)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }
        if (midiIt != midi.cend())
            end = std::min (end, (*midiIt).samplePosition);

        updateVoiceParameters();
        renderVoice (left + pos, right != nullptr ? right + pos : nullptr, end - pos);
        pos = end;
    }
    for (; midiIt != midi.cend(); ++midiIt)
        handleMidi ((*midiIt).getMessage());
}

void RotorAudioProcessor::renderVoice (float* left, float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = voice.process() * smoothedLevel.getNextValue();
        left[i] = s;
        if (right != nullptr)
            right[i] = s;
    }
}

void RotorAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff();
}

void RotorAudioProcessor::noteOn (int note, float velocity)
{
    removeHeld (note); // drop a duplicate of this key from the stack
    if (numHeld < (int) heldNotes.size())
        heldNotes[(size_t) numHeld++] = note;

    // OPEN: velocity response (does the hardware respond to velocity at all?).
    lastVelocity = velocity;
    voice.noteOn (note, velocity);
}

bool RotorAudioProcessor::removeHeld (int note)
{
    for (int i = 0; i < numHeld; ++i)
    {
        if (heldNotes[(size_t) i] != note)
            continue;
        for (int j = i; j < numHeld - 1; ++j)
            heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];
        --numHeld;
        return true;
    }
    return false;
}

void RotorAudioProcessor::noteOff (int note)
{
    if (! removeHeld (note) || voice.getCurrentNote() != note)
        return;

    if (numHeld > 0)
        voice.noteOn (heldNotes[(size_t) numHeld - 1], lastVelocity);
    else
        voice.noteOff();
}

void RotorAudioProcessor::allNotesOff()
{
    numHeld = 0;
    voice.noteOff();
}

juce::AudioProcessorEditor* RotorAudioProcessor::createEditor()
{
    // v1.1: generic editor. The real UI comes in v1.9.
    return new juce::GenericAudioProcessorEditor (*this);
}

void RotorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void RotorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RotorAudioProcessor();
}
