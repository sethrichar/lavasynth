#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace rotor::params
{

// Parameter IDs are part of saved state — never rename one, only add new IDs.
inline constexpr const char* waveform = "waveform";
inline constexpr const char* octave = "octave";
inline constexpr const char* cutoff = "cutoff";
inline constexpr const char* resonance = "resonance";
inline constexpr const char* attack = "attack";
inline constexpr const char* decay = "decay";
inline constexpr const char* sustain = "sustain";
inline constexpr const char* release = "release";
inline constexpr const char* level = "level";

// Bump when a parameter's meaning changes so old sessions can be migrated.
inline constexpr int version = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace rotor::params
