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
    oscLevelParam = state.getRawParameterValue (oscLevel);
    subLevelParam = state.getRawParameterValue (subLevel);
    subOctaveParam = state.getRawParameterValue (subOctave);
    subWaveformParam = state.getRawParameterValue (subWaveform);
    noiseLevelParam = state.getRawParameterValue (noiseLevel);
    noiseColorParam = state.getRawParameterValue (noiseColor);
    glideParam = state.getRawParameterValue (glide);
    filterModeParam = state.getRawParameterValue (filterMode);
    cutoffParam = state.getRawParameterValue (cutoff);
    resonanceParam = state.getRawParameterValue (resonance);
    keyTrackParam = state.getRawParameterValue (keyTrack);
    foldParam = state.getRawParameterValue (fold);
    ampEnvParams = { state.getRawParameterValue (attack), state.getRawParameterValue (decay),
                     state.getRawParameterValue (sustain), state.getRawParameterValue (release),
                     state.getRawParameterValue (ampLoop), state.getRawParameterValue (ampKeyTrack) };
    modEnvParams = { state.getRawParameterValue (modAttack), state.getRawParameterValue (modDecay),
                     state.getRawParameterValue (modSustain), state.getRawParameterValue (modRelease),
                     state.getRawParameterValue (modLoop), state.getRawParameterValue (modKeyTrack) };
    modEnvToPdParam = state.getRawParameterValue (modEnvToPd);
    modEnvToCutoffParam = state.getRawParameterValue (modEnvToCutoff);
    modEnvToLfoRateParam = state.getRawParameterValue (modEnvToLfoRate);
    modEnvToSpreadParam = state.getRawParameterValue (modEnvToSpread);
    modEnvToFoldParam = state.getRawParameterValue (modEnvToFold);
    envSyncParam = state.getRawParameterValue (envSync);
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
        v.prepare (sampleRate * rotor::Decimator4x::factor);
    decimator.reset();
    noise.reset();
    noiseTilt.setSampleRate (sampleRate);
    noiseTilt.reset();
    previousNoise = 0.0f;
    allocator.reset();
    allocator.setGraceSamples ((int) (rotor::VoiceAllocator::defaultGraceSeconds * sampleRate));

    // Control-rate smoothers tick once per control block, the level smoother once per sample.
    const double controlRate = sampleRate / controlBlockSize;
    auto initControl = [controlRate] (auto& smoother, std::atomic<float>* param)
    {
        smoother.reset (controlRate, smoothingSeconds);
        smoother.setCurrentAndTargetValue (param->load());
    };
    initControl (smoothedCutoff, cutoffParam);
    initControl (smoothedResonance, resonanceParam);
    initControl (smoothedOscLevel, oscLevelParam);
    initControl (smoothedSubLevel, subLevelParam);
    initControl (smoothedNoiseLevel, noiseLevelParam);
    initControl (smoothedNoiseColor, noiseColorParam);
    initControl (smoothedFold, foldParam);
    initControl (smoothedModEnvToCutoff, modEnvToCutoffParam);
    initControl (smoothedModEnvToFold, modEnvToFoldParam);
    smoothedLevel.reset (sampleRate, smoothingSeconds);
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

    noiseTilt.setColor (smoothedNoiseColor.getNextValue());

    rotor::Voice::Parameters p;
    p.oscLevel = smoothedOscLevel.getNextValue();
    p.subLevel = smoothedSubLevel.getNextValue();
    p.subOctave = subOctaveParam->load() >= 0.5f ? 2 : 1;
    p.subWaveform = subWaveformParam->load() >= 0.5f ? rotor::Voice::SubWaveform::square : rotor::Voice::SubWaveform::sine;
    p.noiseLevel = smoothedNoiseLevel.getNextValue();
    p.glideSecondsPerOctave = glideParam->load();
    p.filterMode = filterModeParam->load() >= 0.5f ? rotor::LadderFilter::Mode::bandpass : rotor::LadderFilter::Mode::lowpass;
    p.cutoffHz = smoothedCutoff.getNextValue();
    p.resonance = smoothedResonance.getNextValue();
    p.keyTrack = keyTrackParam->load();
    p.fold = smoothedFold.getNextValue();
    p.amp = readEnvelope (ampEnvParams);
    p.mod = readEnvelope (modEnvParams);
    p.ampKeyTrack = ampEnvParams.keyTrack->load();
    p.modKeyTrack = modEnvParams.keyTrack->load();
    p.modEnvDepth.phaseDistortion = modEnvToPdParam->load();
    p.modEnvDepth.cutoff = smoothedModEnvToCutoff.getNextValue();
    p.modEnvDepth.lfoRate = modEnvToLfoRateParam->load();
    p.modEnvDepth.spread = modEnvToSpreadParam->load();
    p.modEnvDepth.fold = smoothedModEnvToFold.getNextValue();

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

rotor::Envelope::Parameters RotorAudioProcessor::readEnvelope (const EnvelopeParams& e) const
{
    rotor::Envelope::Parameters p;
    p.attackSeconds = e.attack->load();
    p.decaySeconds = e.decay->load();
    p.sustainLevel = e.sustain->load();
    p.releaseSeconds = e.release->load();
    p.loop = e.loop->load() >= 0.5f;

    // ENV CLK: snap the stage times to note values at the host tempo.
    if (envSyncParam->load() >= 0.5f)
    {
        p.attackSeconds = rotor::NoteValues::snapSeconds (p.attackSeconds, hostBpm);
        p.decaySeconds = rotor::NoteValues::snapSeconds (p.decaySeconds, hostBpm);
        p.releaseSeconds = rotor::NoteValues::snapSeconds (p.releaseSeconds, hostBpm);
    }
    return p;
}

void RotorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* host = getPlayHead())
        if (auto position = host->getPosition())
            if (auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
                hostBpm = *bpm;

    smoothedCutoff.setTargetValue (cutoffParam->load());
    smoothedResonance.setTargetValue (resonanceParam->load());
    smoothedOscLevel.setTargetValue (oscLevelParam->load());
    smoothedSubLevel.setTargetValue (subLevelParam->load());
    smoothedNoiseLevel.setTargetValue (noiseLevelParam->load());
    smoothedNoiseColor.setTargetValue (noiseColorParam->load());
    smoothedFold.setTargetValue (foldParam->load());
    smoothedModEnvToCutoff.setTargetValue (modEnvToCutoffParam->load());
    smoothedModEnvToFold.setTargetValue (modEnvToFoldParam->load());
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
    constexpr int os = rotor::Decimator4x::factor;
    float block[os];

    for (int i = 0; i < numSamples; ++i)
    {
        // Noise is made at the base rate and linearly interpolated up to the voice rate.
        const float n = noiseTilt.process (noise.process());

        for (int j = 0; j < os; ++j)
        {
            const float noiseSample = previousNoise + (n - previousNoise) * (float) (j + 1) / (float) os;
            float sum = 0.0f;
            for (auto& v : voices)
                sum += v.process (noiseSample);
            block[j] = sum;
        }
        previousNoise = n;

        // v1.6 adds the stereo spreader; until then voices are summed to the centre.
        const float s = decimator.process (block) * voiceSumGain * smoothedLevel.getNextValue();
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
