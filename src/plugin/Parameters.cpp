#include "Parameters.h"

#include "dsp/Envelope.h"

namespace rotor::params
{

namespace
{
    // Exponential time range: equal slider travel = equal ratio of time.
    juce::NormalisableRange<float> timeRange (float minSeconds, float maxSeconds)
    {
        return { minSeconds, maxSeconds,
                 [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
                 [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); } };
    }

    juce::String secondsToText (float seconds, int)
    {
        if (seconds < 1.0f) return juce::String (seconds * 1000.0f, 1) + " ms";
        if (seconds < 60.0f) return juce::String (seconds, 2) + " s";
        return juce::String (seconds / 60.0f, 1) + " min";
    }

    juce::AudioParameterFloatAttributes timeAttributes()
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction (secondsToText);
    }
} // namespace

juce::String voiceOn (int i) { return "voice" + juce::String (i + 1) + "On"; }
juce::String voiceLevel (int i) { return "voice" + juce::String (i + 1) + "Level"; }
juce::String voiceOctave (int i) { return "voice" + juce::String (i + 1) + "Octave"; }
juce::String voiceWaveform (int i) { return "voice" + juce::String (i + 1) + "Waveform"; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { voiceMode, version }, "Voice Mode",
        StringArray { "Forward", "Backward", "Random", "Staccato", "Legato", "Mono" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { roundRobinReset, version }, "Round-Robin Reset", false));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { unisonGrace, version }, "Unison Grace Period", false));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { monoPriority, version }, "Mono Note Priority",
        StringArray { "Last", "Lowest", "Highest" }, 0));

    for (int i = 0; i < rotor::numVoices; ++i)
    {
        const String name = "Voice " + String (i + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { voiceOn (i), version }, name + "On", true));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { voiceLevel (i), version }, name + "Level", NormalisableRange<float> (0.0f, 1.0f), 0.8f));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { voiceOctave (i), version }, name + "Octave", -2, 2, 0));
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { voiceWaveform (i), version }, name + "Waveform",
            StringArray { "Square", "Saw", "Shark-tooth", "Triangle", "Sine" }, 2));
    }

    // Global section
    const auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; });
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { oscLevel, version }, "OSC Level", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return v <= 0.5f ? String (juce::roundToInt (v * 200.0f)) + "%"
                             : "Drive " + String (juce::roundToInt ((v - 0.5f) * 200.0f)) + "%";
        })));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { subLevel, version }, "Sub Level", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { subOctave, version }, "Sub Octave", StringArray { "-1 oct", "-2 oct" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { subWaveform, version }, "Sub Waveform", StringArray { "Sine", "Square" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { noiseLevel, version }, "Noise Level", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { noiseColor, version }, "Noise Color", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            if (std::abs (v) < 0.005f) return String ("Flat");
            return (v < 0.0f ? "Dark " : "Bright ") + String (juce::roundToInt (std::abs (v) * 100.0f)) + "%";
        })));

    // Glide: seconds per octave; 0 = off.
    NormalisableRange<float> glideRange (0.0f, 5.0f);
    glideRange.setSkewForCentre (0.3f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { glide, version }, "Glide", glideRange, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return v <= 0.0f ? String ("Off") : secondsToText (v, 0) + "/oct"; })));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { filterMode, version }, "Filter Mode", StringArray { "Lowpass", "Bandpass" }, 0));

    NormalisableRange<float> cutoffRange (20.0f, 20000.0f);
    cutoffRange.setSkewForCentre (1000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { cutoff, version }, "Cutoff", cutoffRange, 8000.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { resonance, version }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent));
    // 100% = the cutoff follows pitch 1:1 (self-oscillation plays in tune).
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { keyTrack, version }, "Filter Key Track", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));

    // OPEN: wavefolder becomes the "Wavefolder" wildcard slider in v1.6 and a mod destination in v1.5.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { fold, version }, "Wavefolder", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));

    // Envelopes. OPEN: slider curves. Minimum = half the fastest loop (C1); max release ≈ 1 hour.
    const float minTime = (float) rotor::Envelope::minTimeSeconds;
    auto bipolarPercent = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        const int pc = juce::roundToInt (v * 100.0f);
        return pc == 0 ? String ("Off") : (pc > 0 ? "+" : "") + String (pc) + "%";
    });

    auto addEnvelope = [&] (const char* a, const char* d, const char* sus, const char* r, const char* loop,
                            const char* kt, const String& prefix)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { a, version }, prefix + "Attack", timeRange (minTime, 20.0f), minTime, timeAttributes()));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { d, version }, prefix + "Decay", timeRange (minTime, 60.0f), 0.3f, timeAttributes()));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { sus, version }, prefix + "Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.7f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { r, version }, prefix + "Release", timeRange (minTime, 3600.0f), 0.3f, timeAttributes()));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { loop, version }, prefix + "Loop", false));
        // Clockwise (+) = higher notes faster; counter-clockwise (−) = lower notes faster.
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { kt, version }, prefix + "Key Track", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent));
    };

    // IDs "attack"/"decay"/"sustain"/"release" are the amp envelope (kept from v1.1).
    addEnvelope (attack, decay, sustain, release, ampLoop, ampKeyTrack, "Amp ");
    addEnvelope (modAttack, modDecay, modSustain, modRelease, modLoop, modKeyTrack, "Mod Env ");

    // Mod envelope destinations: bipolar depth, centre = off.
    auto addDepth = [&] (const char* id, const String& name)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, version }, name, NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent));
    };
    addDepth (modEnvToPd, "Mod Env > Phase Dist");
    addDepth (modEnvToCutoff, "Mod Env > Cutoff");
    addDepth (modEnvToLfoRate, "Mod Env > LFO Rate");
    addDepth (modEnvToSpread, "Mod Env > Spread");
    addDepth (modEnvToFold, "Mod Env > Wavefolder");

    // ENV CLK: envelope times snap to note values at the host tempo.
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { envSync, version }, "Envelope Sync", false));

    // LFO
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { lfoRate, version }, "LFO Rate", NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { lfoRange, version }, "LFO Range", StringArray { "Slow", "Fast" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { lfoShape, version }, "LFO Shape",
        StringArray { "Volcano", "Square", "Reverse Saw", "Saw", "Sine" }, 4));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { lfoKeyTrack, version }, "LFO Key Track", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { lfoRetrigger, version }, "LFO Retrigger", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { lfoSync, version }, "LFO Sync", false));
    addDepth (lfoToPd, "LFO > Phase Dist");
    addDepth (lfoToCutoff, "LFO > Cutoff");
    addDepth (lfoToSpread, "LFO > Spread");
    addDepth (lfoToFold, "LFO > Wavefolder");

    // Wildcards: eight unipolar sliders, each acting on every voice independently.
    auto addWildcard = [&] (const char* id, const String& name)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, version }, name, NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    };
    addWildcard (noteDetune, "Note Detune");
    addWildcard (wow, "Wow");
    addWildcard (flutter, "Flutter");
    addWildcard (reelDrag, "Reel Drag");
    addWildcard (chaos, "Chaos");
    addWildcard (envScatter, "Envelope Scatter");
    addWildcard (spread, "Stereo Spread");

    // Phase distortion offset for the main oscillators; negative warps the other way.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { phaseDist, version }, "Phase Distortion", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { level, version }, "Master Level", NormalisableRange<float> (0.0f, 1.0f), 0.7f));

    return layout;
}

} // namespace rotor::params
