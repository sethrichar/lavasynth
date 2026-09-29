#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
    // Parameters that are expensive to apply (filter coefficients) are updated every this many samples.
    constexpr int controlBlockSize = 32;
    constexpr double smoothingSeconds = 0.02;

    // Headroom for five summed voices.
    constexpr float voiceSumGain = 0.4f * 1.41421356f; // ×√2 offsets the centre pan's −3 dB
} // namespace

RotorAudioProcessor::RotorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
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
    lfoRateParam = state.getRawParameterValue (lfoRate);
    lfoRangeParam = state.getRawParameterValue (lfoRange);
    lfoShapeParam = state.getRawParameterValue (lfoShape);
    lfoKeyTrackParam = state.getRawParameterValue (lfoKeyTrack);
    lfoRetriggerParam = state.getRawParameterValue (lfoRetrigger);
    lfoSyncParam = state.getRawParameterValue (lfoSync);
    lfoToPdParam = state.getRawParameterValue (lfoToPd);
    lfoToCutoffParam = state.getRawParameterValue (lfoToCutoff);
    lfoToSpreadParam = state.getRawParameterValue (lfoToSpread);
    lfoToFoldParam = state.getRawParameterValue (lfoToFold);
    phaseDistParam = state.getRawParameterValue (phaseDist);
    noteDetuneParam = state.getRawParameterValue (noteDetune);
    wowParam = state.getRawParameterValue (wow);
    flutterParam = state.getRawParameterValue (flutter);
    reelDragParam = state.getRawParameterValue (reelDrag);
    chaosParam = state.getRawParameterValue (chaos);
    envScatterParam = state.getRawParameterValue (envScatter);
    spreadParam = state.getRawParameterValue (spread);
    envCurveParam = state.getRawParameterValue (envCurve);
    reverbAmountParam = state.getRawParameterValue (reverbAmount);
    reverbMixParam = state.getRawParameterValue (reverbMix);
    driveAmountParam = state.getRawParameterValue (driveAmount);
    driveMixParam = state.getRawParameterValue (driveMix);
    fxColorParam = state.getRawParameterValue (fxColor);
    lfoToReverbMixParam = state.getRawParameterValue (lfoToReverbMix);
    lfoToDriveMixParam = state.getRawParameterValue (lfoToDriveMix);
    lfoToFxAmountParam = state.getRawParameterValue (lfoToFxAmount);
    lfoToFxColorParam = state.getRawParameterValue (lfoToFxColor);
    atWildcardParam = state.getRawParameterValue (atWildcard);
    atCutoffParam = state.getRawParameterValue (atCutoff);
    atLfoRateParam = state.getRawParameterValue (atLfoRate);
    mpeParam = state.getRawParameterValue (mpe);
    modWheelModeParam = state.getRawParameterValue (modWheelMode);
    tuneModeParam = state.getRawParameterValue (tuneMode);
    tuneParam = state.getRawParameterValue (tune);
    extInputParam = state.getRawParameterValue (extInput);
    noteChannel.fill (1);
    voiceChannel.fill (1);
    filterCharacterParam = state.getRawParameterValue (filterCharacter);

    // Each voice: its spreader position and its own random sequences (LFO, wildcards).
    for (int i = 0; i < rotor::numVoices; ++i)
        voices[(size_t) i].setIdentity (i, 0x9E3779B9u * (std::uint32_t) (i + 1));
    levelParam = state.getRawParameterValue (level);
}

bool RotorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    // Sidechain (EXT): off, mono (input 1 → every voice) or stereo (see extSourceFor).
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void RotorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    extBuffer.setSize (2, std::max (1, samplesPerBlock));
    extBuffer.clear();
    extTiltLeft.setSampleRate (sampleRate);
    extTiltRight.setSampleRate (sampleRate);
    extTiltLeft.reset();
    extTiltRight.reset();
    std::fill (std::begin (previousExt), std::end (previousExt), 0.0f);
    expression.reset();

    for (auto& v : voices)
        v.prepare (sampleRate * rotor::Decimator4x::factor);
    decimatorLeft.reset();
    decimatorRight.reset();
    reverb.prepare (sampleRate);
    driveLeft.prepare (sampleRate);
    driveRight.prepare (sampleRate);
    noise.reset();
    noiseTilt.setSampleRate (sampleRate);
    noiseTilt.reset();
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
    initControl (smoothedLfoRate, lfoRateParam);
    initControl (smoothedLfoToPd, lfoToPdParam);
    initControl (smoothedLfoToCutoff, lfoToCutoffParam);
    initControl (smoothedLfoToFold, lfoToFoldParam);
    initControl (smoothedModEnvToPd, modEnvToPdParam);
    initControl (smoothedPhaseDist, phaseDistParam);
    initControl (smoothedSpread, spreadParam);
    initControl (smoothedReverbAmount, reverbAmountParam);
    initControl (smoothedReverbMix, reverbMixParam);
    initControl (smoothedDriveAmount, driveAmountParam);
    initControl (smoothedDriveMix, driveMixParam);
    initControl (smoothedFxColor, fxColorParam);
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

    const float color = smoothedNoiseColor.getNextValue();
    noiseTilt.setColor (color);
    extTiltLeft.setColor (color); // EXT goes through the same tilt EQ
    extTiltRight.setColor (color);
    expression.setMpe (mpeParam->load() >= 0.5f);

    rotor::Voice::Parameters p;
    p.oscLevel = smoothedOscLevel.getNextValue();
    p.subLevel = smoothedSubLevel.getNextValue();
    p.subOctave = subOctaveParam->load() >= 0.5f ? 2 : 1;
    p.subWaveform = subWaveformParam->load() >= 0.5f ? rotor::Voice::SubWaveform::square : rotor::Voice::SubWaveform::sine;
    p.noiseLevel = smoothedNoiseLevel.getNextValue();
    p.glideSecondsPerOctave = glideParam->load();
    p.filterMode = filterModeParam->load() >= 0.5f ? rotor::LadderFilter::Mode::bandpass : rotor::LadderFilter::Mode::lowpass;
    p.filterCharacter = static_cast<rotor::LadderFilter::Character> (juce::jlimit (0, 4, (int) filterCharacterParam->load()));
    p.cutoffHz = smoothedCutoff.getNextValue();
    p.resonance = smoothedResonance.getNextValue();
    p.keyTrack = keyTrackParam->load();
    p.fold = smoothedFold.getNextValue();
    p.amp = readEnvelope (ampEnvParams);
    p.mod = readEnvelope (modEnvParams);
    p.ampKeyTrack = ampEnvParams.keyTrack->load();
    p.modKeyTrack = modEnvParams.keyTrack->load();
    p.modEnvDepth.phaseDistortion = smoothedModEnvToPd.getNextValue();
    p.modEnvDepth.cutoff = smoothedModEnvToCutoff.getNextValue();
    p.modEnvDepth.lfoRate = modEnvToLfoRateParam->load();
    p.modEnvDepth.spread = modEnvToSpreadParam->load();
    p.modEnvDepth.fold = smoothedModEnvToFold.getNextValue();
    p.phaseDistortion = smoothedPhaseDist.getNextValue();

    const auto range = lfoRangeParam->load() >= 0.5f ? rotor::Lfo::Range::fast : rotor::Lfo::Range::slow;
    double lfoHz = rotor::Lfo::rateToHz (smoothedLfoRate.getNextValue(), range);
    if (lfoSyncParam->load() >= 0.5f) // LFO CLK: the period snaps to a note value
        lfoHz = 1.0 / rotor::NoteValues::snapSeconds (1.0 / lfoHz, hostBpm);
    p.lfo.rateHz = lfoHz;
    p.lfo.shape = static_cast<rotor::Lfo::Shape> (juce::jlimit (0, 4, (int) lfoShapeParam->load()));
    p.lfo.keyTrack = lfoKeyTrackParam->load();
    p.lfo.retrigger = lfoRetriggerParam->load() >= 0.5f;
    p.lfoDepth.phaseDistortion = smoothedLfoToPd.getNextValue();
    p.lfoDepth.cutoff = smoothedLfoToCutoff.getNextValue();
    p.lfoDepth.spread = lfoToSpreadParam->load();
    p.lfoDepth.fold = smoothedLfoToFold.getNextValue();

    p.wild.noteDetune = noteDetuneParam->load();
    p.wild.wow = wowParam->load();
    p.wild.flutter = flutterParam->load();
    p.wild.reelDrag = reelDragParam->load();
    p.wild.chaos = chaosParam->load();
    p.wild.envScatter = envScatterParam->load();
    p.spread = smoothedSpread.getNextValue();

    p.atWildcard = atWildcardParam->load();
    p.atCutoff = atCutoffParam->load();
    p.atLfoRate = atLfoRateParam->load();
    const bool wheelPitchLfo = modWheelModeParam->load() >= 0.5f;
    // OPEN: which wildcards the wheel blends in and how deep (assumed: the four pitch wildcards, fully).
    p.wheelWildcardBlend = wheelPitchLfo ? 0.0 : expression.getWheel();
    p.wheelPitchLfo = wheelPitchLfo ? expression.getWheel() : 0.0;
    const double tuneValue = tuneParam->load();
    if (tuneModeParam->load() >= 0.5f) // Pitch Drift: + = drift, − = global detune
    {
        p.pitchDrift = std::max (0.0, tuneValue);
        p.globalDetune = std::min (0.0, tuneValue);
    }
    else
    {
        p.globalDetune = tuneValue;
    }

    for (int i = 0; i < rotor::numVoices; ++i)
    {
        const auto& vp = voiceParams[(size_t) i];
        p.waveform = static_cast<rotor::Waveform> (juce::jlimit (0, rotor::numWaveforms - 1, (int) vp.waveform->load()));
        p.octave = juce::jlimit (-2, 2, juce::roundToInt (vp.octave->load()));
        p.level = vp.level->load();
        voices[(size_t) i].setExpression (expression.forChannel (voiceChannel[(size_t) i]));
        voices[(size_t) i].setParameters (p);
    }

    // After the voices have their new settings, so a voice switched on joins with them.
    for (int i = 0; i < rotor::numVoices; ++i)
        apply (allocator.setVoiceEnabled (i, voiceParams[(size_t) i].on->load() >= 0.5f));
}

void RotorAudioProcessor::updateEffects()
{
    // LFO min-maxing: the five voice LFOs combined (up = maximum, down = minimum).
    std::array<float, rotor::numVoices> lfos {};
    for (int i = 0; i < rotor::numVoices; ++i)
        lfos[(size_t) i] = voices[(size_t) i].getLfoValue();
    const auto mod = [&lfos] (std::atomic<float>* depth) { return rotor::lfoMinMax (lfos, depth->load()); };

    // OPEN: "effects amount" is assumed to move both Reverb Amount and Drive Amount.
    const double amountMod = mod (lfoToFxAmountParam);
    const double color = juce::jlimit (-1.0, 1.0, smoothedFxColor.getNextValue() + mod (lfoToFxColorParam));
    auto unit = [] (double x) { return juce::jlimit (0.0, 1.0, x); };

    reverb.setParameters (unit (smoothedReverbAmount.getNextValue() + amountMod), color,
                          unit (smoothedReverbMix.getNextValue() + mod (lfoToReverbMixParam)));
    const double driveAmt = unit (smoothedDriveAmount.getNextValue() + amountMod);
    const double driveWet = unit (smoothedDriveMix.getNextValue() + mod (lfoToDriveMixParam));
    driveLeft.setParameters (driveAmt, color, driveWet);
    driveRight.setParameters (driveAmt, color, driveWet);
}

rotor::Envelope::Parameters RotorAudioProcessor::readEnvelope (const EnvelopeParams& e) const
{
    rotor::Envelope::Parameters p;
    p.attackSeconds = e.attack->load();
    p.decaySeconds = e.decay->load();
    p.sustainLevel = e.sustain->load();
    p.releaseSeconds = e.release->load();
    p.loop = e.loop->load() >= 0.5f;
    p.curve = static_cast<rotor::Envelope::Curve> (juce::jlimit (0, 4, (int) envCurveParam->load()));

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

    // Copy the sidechain before the buffer (shared with the output) is cleared.
    {
        const auto input = getBusBuffer (buffer, true, 0);
        extChannels = std::min (input.getNumChannels(), 2);
        const int n = std::min (buffer.getNumSamples(), extBuffer.getNumSamples());
        for (int ch = 0; ch < extChannels; ++ch)
            extBuffer.copyFrom (ch, 0, input, ch, 0, n);
    }
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
    smoothedLfoRate.setTargetValue (lfoRateParam->load());
    smoothedLfoToPd.setTargetValue (lfoToPdParam->load());
    smoothedLfoToCutoff.setTargetValue (lfoToCutoffParam->load());
    smoothedLfoToFold.setTargetValue (lfoToFoldParam->load());
    smoothedModEnvToPd.setTargetValue (modEnvToPdParam->load());
    smoothedPhaseDist.setTargetValue (phaseDistParam->load());
    smoothedSpread.setTargetValue (spreadParam->load());
    smoothedReverbAmount.setTargetValue (reverbAmountParam->load());
    smoothedReverbMix.setTargetValue (reverbMixParam->load());
    smoothedDriveAmount.setTargetValue (driveAmountParam->load());
    smoothedDriveMix.setTargetValue (driveMixParam->load());
    smoothedFxColor.setTargetValue (fxColorParam->load());
    smoothedLevel.setTargetValue (levelParam->load());

    const int numSamples = buffer.getNumSamples();
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto midiIt = midi.cbegin();
    int pos = 0;
    while (pos < numSamples)
    {
        // Settings first (a voice-mode change releases voices, so it must not come after new notes).
        updateVoiceParameters();

        // Split at control-block boundaries and at MIDI events for sample-accurate notes.
        int end = std::min (numSamples, pos + controlBlockSize);
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= pos)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }
        if (midiIt != midi.cend())
            end = std::min (end, (*midiIt).samplePosition);

        updateEffects();
        render (left + pos, right != nullptr ? right + pos : nullptr, end - pos, pos);
        apply (allocator.advance (end - pos));
        pos = end;
    }
    for (; midiIt != midi.cend(); ++midiIt)
        handleMidi ((*midiIt).getMessage());
}

void RotorAudioProcessor::render (float* left, float* right, int numSamples, int offset)
{
    constexpr int os = rotor::Decimator4x::factor;
    float blockLeft[os], blockRight[os];

    const bool ext = extInputParam->load() >= 0.5f && extChannels > 0;
    const bool stereoExt = ext && extChannels == 2;
    std::array<rotor::ExtSource, rotor::numVoices> sources {};
    for (int v = 0; v < rotor::numVoices; ++v)
        sources[(size_t) v] = rotor::extSourceFor (v, stereoExt);

    for (int i = 0; i < numSamples; ++i)
    {
        // Voice input: shared pink noise, or the EXT input (mono / left / right), tilt-EQ'd at the
        // base rate and linearly interpolated up to the voice rate.
        float now[3];
        if (ext)
        {
            const int idx = std::min (offset + i, extBuffer.getNumSamples() - 1);
            const float inL = extBuffer.getSample (0, idx);
            const float inR = extChannels > 1 ? extBuffer.getSample (1, idx) : inL;
            now[1] = extTiltLeft.process (inL);
            now[2] = extTiltRight.process (inR);
            now[0] = 0.5f * (now[1] + now[2]);
        }
        else
        {
            now[0] = now[1] = now[2] = noiseTilt.process (noise.process());
        }

        for (int j = 0; j < os; ++j)
        {
            const float t = (float) (j + 1) / (float) os;
            float in[3];
            for (int k = 0; k < 3; ++k)
                in[k] = previousExt[k] + (now[k] - previousExt[k]) * t;

            float sumLeft = 0.0f, sumRight = 0.0f;
            for (int v = 0; v < rotor::numVoices; ++v)
            {
                auto& voice = voices[(size_t) v];
                const float s = voice.process (in[(int) sources[(size_t) v]]);
                sumLeft += s * voice.getPanLeft();
                sumRight += s * voice.getPanRight();
            }
            blockLeft[j] = sumLeft;
            blockRight[j] = sumRight;
        }
        for (int k = 0; k < 3; ++k)
            previousExt[k] = now[k];

        float l = decimatorLeft.process (blockLeft) * voiceSumGain;
        float r = decimatorRight.process (blockRight) * voiceSumGain;

        // Effects: reverb, then CMOS drive (per the manual), then master level.
        reverb.process (l, r);
        l = driveLeft.process (l);
        r = driveRight.process (r);
        const float master = smoothedLevel.getNextValue();
        l *= master;
        r *= master;
        if (right != nullptr)
        {
            left[i] = l;
            right[i] = r;
        }
        else
        {
            left[i] = (l + r) * 0.70710678f; // mono bus: centred voices keep their level
        }
    }
}

void RotorAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    const int channel = m.getChannel();
    if (m.isNoteOn())
    {
        // OPEN: velocity response (does the hardware respond to velocity at all?).
        lastVelocity = m.getFloatVelocity();
        expression.noteOn (channel);
        noteChannel[(size_t) m.getNoteNumber()] = channel;
        apply (allocator.noteOn (m.getNoteNumber()));
    }
    else if (m.isPitchWheel())
    {
        expression.pitchBend (channel, m.getPitchWheelValue());
    }
    else if (m.isChannelPressure())
    {
        expression.channelPressure (channel, m.getChannelPressureValue());
    }
    else if (m.isAftertouch()) // polyphonic aftertouch: treated as that channel's pressure
    {
        expression.channelPressure (channel, m.getAfterTouchValue());
    }
    else if (m.isController() && ! m.isAllNotesOff() && ! m.isAllSoundOff())
    {
        expression.controller (channel, m.getControllerNumber(), m.getControllerValue());
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
            case Type::trigger:
                voiceChannel[(size_t) i] = noteChannel[(size_t) action.note];
                voice.setExpression (expression.forChannel (voiceChannel[(size_t) i]));
                voice.noteOn (action.note, lastVelocity);
                break;
            case Type::move:
                voiceChannel[(size_t) i] = noteChannel[(size_t) action.note];
                voice.moveTo (action.note);
                break;
            case Type::release: voice.noteOff(); break;
        }
    }
}

juce::AudioProcessorEditor* RotorAudioProcessor::createEditor()
{
    return new RotorEditor (*this);
}

// State: the parameters plus the row preset memory and preset name, in one XML document.
// (v1.1–v1.8 saved the parameter tree alone; those sessions still load.)
void RotorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("RotorPlugin");
    root.setAttribute ("stateVersion", 2);
    if (auto params = state.copyState().createXml())
        root.addChildElement (params.release());
    if (auto rows = presetManager.toValueTree().createXml())
        root.addChildElement (rows.release());
    copyXmlToBinary (root, destData);
}

void RotorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const juce::XmlElement* params = xml.get();
    if (xml->hasTagName ("RotorPlugin"))
    {
        params = xml->getChildByName (state.state.getType());
        if (auto* rows = xml->getChildByName ("RowPresets"))
            presetManager.fromValueTree (juce::ValueTree::fromXml (*rows));
    }
    if (params != nullptr && params->hasTagName (state.state.getType()))
    {
        state.replaceState (juce::ValueTree::fromXml (*params));

        // replaceState skips parameters whose stored value "looks unchanged" — but a host can leave a
        // toggle or choice at an in-between raw value (e.g. 0.72 for "on"). Push every stored value
        // explicitly so the parameters match the saved state exactly.
        for (const auto& child : state.state)
        {
            const auto id = child.getProperty ("id").toString();
            if (auto* param = state.getParameter (id); param != nullptr && child.hasProperty ("value"))
            {
                const float normalised = param->convertTo0to1 ((float) (double) child.getProperty ("value"));
                if (std::abs (param->getValue() - normalised) > 1e-6f)
                    param->setValueNotifyingHost (normalised);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RotorAudioProcessor();
}
