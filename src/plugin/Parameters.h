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
inline constexpr const char* unisonGrace = "unisonGrace";
inline constexpr const char* monoPriority = "monoPriority";

// Per voice: IDs are e.g. "voice1Level" … "voice5Level" (1-based, as on the panel).
juce::String voiceOn (int voiceIndex);
juce::String voiceLevel (int voiceIndex);
juce::String voiceOctave (int voiceIndex);
juce::String voiceWaveform (int voiceIndex);

// Global
inline constexpr const char* oscLevel = "oscLevel";
inline constexpr const char* subLevel = "subLevel";
inline constexpr const char* subOctave = "subOctave";
inline constexpr const char* subWaveform = "subWaveform";
inline constexpr const char* noiseLevel = "noiseLevel";
inline constexpr const char* noiseColor = "noiseColor";
inline constexpr const char* glide = "glide";

// Filter
inline constexpr const char* filterMode = "filterMode";
inline constexpr const char* cutoff = "cutoff";
inline constexpr const char* resonance = "resonance";
inline constexpr const char* keyTrack = "keyTrack";

// Wavefolder
inline constexpr const char* fold = "fold";

// Amp envelope
inline constexpr const char* attack = "attack";
inline constexpr const char* decay = "decay";
inline constexpr const char* sustain = "sustain";
inline constexpr const char* release = "release";
inline constexpr const char* ampLoop = "ampLoop";
inline constexpr const char* ampKeyTrack = "ampKeyTrack";

// Mod envelope
inline constexpr const char* modAttack = "modAttack";
inline constexpr const char* modDecay = "modDecay";
inline constexpr const char* modSustain = "modSustain";
inline constexpr const char* modRelease = "modRelease";
inline constexpr const char* modLoop = "modLoop";
inline constexpr const char* modKeyTrack = "modKeyTrack";
inline constexpr const char* modEnvToPd = "modEnvToPd";
inline constexpr const char* modEnvToCutoff = "modEnvToCutoff";
inline constexpr const char* modEnvToLfoRate = "modEnvToLfoRate";
inline constexpr const char* modEnvToSpread = "modEnvToSpread";
inline constexpr const char* modEnvToFold = "modEnvToFold";

// Host sync (ENV CLK)
inline constexpr const char* envSync = "envSync";

// LFO (one per voice, shared controls)
inline constexpr const char* lfoRate = "lfoRate";
inline constexpr const char* lfoRange = "lfoRange";
inline constexpr const char* lfoShape = "lfoShape";
inline constexpr const char* lfoKeyTrack = "lfoKeyTrack";
inline constexpr const char* lfoRetrigger = "lfoRetrigger";
inline constexpr const char* lfoSync = "lfoSync";
inline constexpr const char* lfoToPd = "lfoToPd";
inline constexpr const char* lfoToCutoff = "lfoToCutoff";
inline constexpr const char* lfoToSpread = "lfoToSpread";
inline constexpr const char* lfoToFold = "lfoToFold";

// Voicing lab (temporary A/B selectors — one choice will be locked in, then these are retired)
inline constexpr const char* envCurve = "envCurve";
inline constexpr const char* filterCharacter = "filterCharacter";

// Wildcards (unipolar, 0 = off). The wavefolder wildcard is `fold`.
inline constexpr const char* noteDetune = "noteDetune";
inline constexpr const char* wow = "wow";
inline constexpr const char* flutter = "flutter";
inline constexpr const char* reelDrag = "reelDrag";
inline constexpr const char* chaos = "chaos";
inline constexpr const char* envScatter = "envScatter";
inline constexpr const char* spread = "spread";

// Phase distortion offset (OPEN: may be modulation-only on the hardware)
inline constexpr const char* phaseDist = "phaseDist";

// Master
inline constexpr const char* level = "level";

// Bump when a parameter's meaning changes so old sessions can be migrated.
inline constexpr int version = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace rotor::params
