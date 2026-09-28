#pragma once

#include "dsp/VoiceAllocator.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace rotor::params
{

// Parameter IDs are part of saved state — never rename one, only add new IDs.
// (v1.1's single-voice "waveform" and "octave" were retired in v1.2 for the per-voice versions.)

// Voice section
inline constexpr const char* voiceMode = "voiceMode";
inline constexpr const char* roundRobinReset = "roundRobinReset";

// Per voice: IDs are e.g. "voice1Level" … "voice5Level" (1-based, as on the panel).
juce::String voiceOn (int voiceIndex);
juce::String voiceLevel (int voiceIndex);
juce::String voiceOctave (int voiceIndex);
juce::String voiceWaveform (int voiceIndex);

// Global
inline constexpr const char* glide = "glide";

// Filter
inline constexpr const char* cutoff = "cutoff";
inline constexpr const char* resonance = "resonance";

// Amp envelope
inline constexpr const char* attack = "attack";
inline constexpr const char* decay = "decay";
inline constexpr const char* sustain = "sustain";
inline constexpr const char* release = "release";

// Master
inline constexpr const char* level = "level";

// Bump when a parameter's meaning changes so old sessions can be migrated.
inline constexpr int version = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace rotor::params
