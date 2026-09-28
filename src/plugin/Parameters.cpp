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

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { waveform, version }, "Waveform",
        StringArray { "Square", "Saw", "Shark-tooth", "Triangle", "Sine" }, 2));

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { octave, version }, "Octave", -2, 2, 0));

    NormalisableRange<float> cutoffRange (20.0f, 20000.0f);
    cutoffRange.setSkewForCentre (1000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { cutoff, version }, "Cutoff", cutoffRange, 8000.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { resonance, version }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f));

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
        ParameterID { level, version }, "Level", NormalisableRange<float> (0.0f, 1.0f), 0.7f));

    return layout;
}

} // namespace rotor::params
