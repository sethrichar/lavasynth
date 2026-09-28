#include "PluginProcessor.h"

namespace
{
    // Parameters that are expensive to apply (filter coefficients) are updated every this many samples.
    constexpr int controlBlockSize = 32;
    constexpr double smoothingSeconds = 0.02;

    // Headroom for five summed voices.
    constexpr float voiceSumGain = 0.4f;
} // namespace

RotorAudioProcessor::RotorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "RotorState", rotor::params::createLayout())
{
    using namespace rotor::params;
    for (int i = 0; i < rotor::numVoices; ++i)
    {
        auto& vp = voiceParams[(size_t) i];
        vp.on = state.getRawParameterValue (voiceOn (i));
        vp.level = state.getRawParameterValue (voiceLevel (i));
        vp.octave = state.getRawParameterValue (voiceOctave (i));
        vp.waveform = state.getRawParameterValue (voiceWaveform (i));
    }
    voiceModeParam = state.getRawParameterValue (voiceMode);
    roundRobinResetParam = state.getRawParameterValue (roundRobinReset);
    unisonGraceParam = state.getRawParameterValue (unisonGrace);
    monoPriorityParam = state.getRawParameterValue (monoPriority);
    glideParam = state.getRawParameterValue (glide);
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
    for (auto& v : voices)
        v.prepare (sampleRate);
    allocator.reset();
    allocator.setGraceSamples ((int) (rotor::VoiceAllocator::defaultGraceSeconds * sampleRate));

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
    using Allocator = rotor::VoiceAllocator;
    apply (allocator.setMode (static_cast<Allocator::Mode> (juce::jlimit (0, 5, (int) voiceModeParam->load()))));
    allocator.setRoundRobinReset (roundRobinResetParam->load() >= 0.5f);
    allocator.setGracePeriod (unisonGraceParam->load() >= 0.5f);
    allocator.setMonoPriority (static_cast<Allocator::MonoPriority> (juce::jlimit (0, 2, (int) monoPriorityParam->load())));

    rotor::Voice::Parameters p;
    p.cutoffHz = smoothedCutoff.getNextValue();
    p.resonance = smoothedResonance.getNextValue();
    p.glideSecondsPerOctave = glideParam->load();
    p.amp.attackSeconds = attackParam->load();
    p.amp.decaySeconds = decayParam->load();
    p.amp.sustainLevel = sustainParam->load();
    p.amp.releaseSeconds = releaseParam->load();

    for (int i = 0; i < rotor::numVoices; ++i)
    {
        const auto& vp = voiceParams[(size_t) i];
        p.waveform = static_cast<rotor::Waveform> (juce::jlimit (0, rotor::numWaveforms - 1, (int) vp.waveform->load()));
        p.octave = juce::jlimit (-2, 2, juce::roundToInt (vp.octave->load()));
        p.level = vp.level->load();
        voices[(size_t) i].setParameters (p);
    }

    // After the voices have their new settings, so a voice switched on joins with them.
    for (int i = 0; i < rotor::numVoices; ++i)
        apply (allocator.setVoiceEnabled (i, voiceParams[(size_t) i].on->load() >= 0.5f));
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
        render (left + pos, right != nullptr ? right + pos : nullptr, end - pos);
        apply (allocator.advance (end - pos));
        pos = end;
    }
    for (; midiIt != midi.cend(); ++midiIt)
        handleMidi ((*midiIt).getMessage());
}

void RotorAudioProcessor::render (float* left, float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        float sum = 0.0f;
        for (auto& v : voices)
            sum += v.process();

        // v1.6 adds the stereo spreader; until then voices are summed to the centre.
        const float s = sum * voiceSumGain * smoothedLevel.getNextValue();
        left[i] = s;
        if (right != nullptr)
            right[i] = s;
    }
}

void RotorAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        // OPEN: velocity response (does the hardware respond to velocity at all?).
        lastVelocity = m.getFloatVelocity();
        apply (allocator.noteOn (m.getNoteNumber()));
    }
    else if (m.isNoteOff())
    {
        apply (allocator.noteOff (m.getNoteNumber()));
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        apply (allocator.allNotesOff());
    }
}

void RotorAudioProcessor::apply (const rotor::VoiceAllocator::Result& result)
{
    using Type = rotor::VoiceAllocator::Action::Type;
    for (int i = 0; i < rotor::numVoices; ++i)
    {
        const auto& action = result.actions[(size_t) i];
        auto& voice = voices[(size_t) i];
        switch (action.type)
        {
            case Type::none: break;
            case Type::trigger: voice.noteOn (action.note, lastVelocity); break;
            case Type::move: voice.moveTo (action.note); break;
            case Type::release: voice.noteOff(); break;
        }
    }
}

juce::AudioProcessorEditor* RotorAudioProcessor::createEditor()
{
    // Generic editor until the UI pass in v1.9.
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
