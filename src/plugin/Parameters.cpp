#include "Parameters.h"

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

    // OPEN: envelope min times / slider curves. Max release ≈ 1 hour per the manual.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { attack, version }, "Attack", timeRange (0.001f, 20.0f), 0.005f, timeAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { decay, version }, "Decay", timeRange (0.001f, 60.0f), 0.3f, timeAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { sustain, version }, "Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { release, version }, "Release", timeRange (0.001f, 3600.0f), 0.3f, timeAttributes()));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { level, version }, "Master Level", NormalisableRange<float> (0.0f, 1.0f), 0.7f));

    return layout;
}

} // namespace rotor::params
