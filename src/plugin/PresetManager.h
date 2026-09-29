#pragma once

#include "PresetRows.h"

#include <juce_audio_processors/juce_audio_processors.h>

// Full presets (factory + user files) and the 3 × 8 row preset memory.
// Everything here runs on the message thread (UI / host program changes), never the audio thread.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    // ---- Full presets --------------------------------------------------------------------
    // The list is factory presets first, then the user's saved presets (alphabetical).
    juce::StringArray getPresetNames() const;
    int getNumFactoryPresets() const;
    void loadPreset (int index);
    void loadNext() { loadPreset ((currentIndex + 1) % std::max (1, getPresetNames().size())); }
    void loadPrevious()
    {
        const int n = std::max (1, getPresetNames().size());
        loadPreset ((currentIndex + n - 1) % n);
    }
    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const { return currentName; }

    // Saves the current panel as a user preset (overwrites a same-named file). Returns success.
    bool saveUserPreset (const juce::String& name);
    static juce::File getUserPresetFolder();

    // ---- Row presets ---------------------------------------------------------------------
    // FULL mode: a slot recalls/stores all three rows; ROW mode: just the given row.
    void setFullMode (bool full) { fullMode = full; }
    bool isFullMode() const { return fullMode; }
    void storeSlot (rotor::presets::Row row, int slot);
    void recallSlot (rotor::presets::Row row, int slot);
    bool isSlotFilled (rotor::presets::Row row, int slot) const { return bank.isFilled (row, slot); }

    // ---- State ---------------------------------------------------------------------------
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

    // Current values of every parameter (real units).
    rotor::presets::Snapshot snapshot() const;
    // Applies values (real units) as host-visible parameter changes. Global options are skipped
    // unless includeGlobal (host state restores them; presets leave the player's setup alone).
    void apply (const rotor::presets::Snapshot& values, bool includeGlobal = false);

    struct FactoryPreset
    {
        const char* name;
        rotor::presets::Snapshot values; // overrides on top of the defaults
    };
    static const std::vector<FactoryPreset>& factoryPresets();

private:
    void resetToDefaults();
    juce::Array<juce::File> userPresetFiles() const;

    juce::AudioProcessorValueTreeState& state;
    rotor::presets::RowPresetBank bank;
    bool fullMode = true;
    int currentIndex = 0;
    juce::String currentName { "Init" };
};
