#include "PresetManager.h"

#include "Randomizer.h"

using rotor::presets::Row;
using rotor::presets::rowOf;
using rotor::presets::Snapshot;

namespace
{
    const juce::String presetExtension = ".rotorpreset";
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

// Factory presets: a few starting points that show off the voice rotation, wildcards and effects.
// Values are in real units; anything not listed stays at its default.
const std::vector<PresetManager::FactoryPreset>& PresetManager::factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },
        { "Glass Rotor",
          { { "voice1Waveform", 4 }, { "voice2Waveform", 3 }, { "voice3Waveform", 4 }, { "voice4Waveform", 3 },
            { "voice5Waveform", 4 }, { "voice2Octave", 1 }, { "voice4Octave", -1 }, { "cutoff", 5000 },
            { "attack", 0.02f }, { "decay", 1.2f }, { "sustain", 0.3f }, { "release", 1.8f }, { "wow", 0.15f },
            { "spread", 0.8f }, { "reverbAmount", 0.75f }, { "reverbMix", 0.35f } } },
        { "Tape Choir",
          { { "voiceMode", 4 }, { "unisonGrace", 1 }, { "voice1Waveform", 2 }, { "voice2Waveform", 1 },
            { "voice3Waveform", 2 }, { "voice4Waveform", 1 }, { "voice5Waveform", 2 }, { "cutoff", 1800 },
            { "resonance", 0.2f }, { "attack", 0.4f }, { "release", 2.5f }, { "wow", 0.45f }, { "flutter", 0.3f },
            { "reelDrag", 0.25f }, { "noteDetune", 0.3f }, { "spread", 1.0f }, { "reverbAmount", 0.9f },
            { "reverbMix", 0.45f }, { "glide", 0.08f } } },
        { "Fold Bass",
          { { "voiceMode", 5 }, { "voice1Waveform", 0 }, { "voice2Waveform", 1 }, { "voice3Waveform", 4 },
            { "voice3Octave", -1 }, { "subLevel", 0.6f }, { "cutoff", 400 }, { "resonance", 0.45f },
            { "decay", 0.35f }, { "sustain", 0.5f }, { "release", 0.12f }, { "fold", 0.25f }, { "modDecay", 0.25f },
            { "modSustain", 0.0f }, { "modEnvToCutoff", 0.5f }, { "modEnvToFold", 0.5f }, { "driveAmount", 0.45f },
            { "driveMix", 0.4f }, { "filterCharacter", 1 } } },
        { "Looping Pluck",
          { { "voice1Waveform", 1 }, { "voice2Waveform", 1 }, { "voice3Waveform", 1 }, { "voice4Waveform", 1 },
            { "voice5Waveform", 1 }, { "ampLoop", 1 }, { "attack", 0.02f }, { "decay", 0.09f }, { "sustain", 0.2f },
            { "ampKeyTrack", 0.6f }, { "cutoff", 1500 }, { "resonance", 0.4f }, { "modDecay", 0.6f },
            { "modSustain", 0.0f }, { "modEnvToCutoff", 0.35f }, { "reverbMix", 0.25f }, { "spread", 0.6f } } },
        { "Volcano Pad",
          { { "voice1Waveform", 2 }, { "voice2Waveform", 3 }, { "voice3Waveform", 1 }, { "voice4Waveform", 3 },
            { "voice5Waveform", 2 }, { "attack", 1.5f }, { "release", 4.0f }, { "cutoff", 900 }, { "resonance", 0.35f },
            { "lfoShape", 0 }, { "lfoRate", 0.55f }, { "lfoKeyTrack", 0.4f }, { "lfoToCutoff", 0.35f },
            { "lfoToPd", 0.45f }, { "lfoToSpread", 0.5f }, { "spread", 0.5f }, { "reverbAmount", 0.85f },
            { "reverbMix", 0.4f }, { "lfoToReverbMix", 0.3f }, { "envCurve", 2 } } },
        { "Screaming Lead",
          { { "voiceMode", 5 }, { "voice1Waveform", 1 }, { "voice2Waveform", 1 }, { "voice2Octave", 1 },
            { "voice3Waveform", 0 }, { "glide", 0.12f }, { "cutoff", 1200 }, { "resonance", 0.8f },
            { "filterCharacter", 4 }, { "keyTrack", 0.5f }, { "modDecay", 0.5f }, { "modSustain", 0.3f },
            { "modEnvToCutoff", 0.3f }, { "driveAmount", 0.6f }, { "driveMix", 0.5f }, { "reverbMix", 0.2f } } },
        { "Cluster Keys",
          { { "voice1Waveform", 3 }, { "voice2Waveform", 2 }, { "voice3Waveform", 4 }, { "voice4Waveform", 2 },
            { "voice5Waveform", 3 }, { "atWildcard", -0.6f }, { "cutoff", 2500 }, { "decay", 0.8f },
            { "sustain", 0.4f }, { "release", 1.2f }, { "envCurve", 1 }, { "spread", 0.7f }, { "reverbAmount", 0.6f },
            { "reverbMix", 0.3f }, { "chaos", 0.1f } } },
    };
    return presets;
}

int PresetManager::getNumFactoryPresets() const { return (int) factoryPresets().size(); }

juce::File PresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Rotor").getChildFile ("Presets");
}

juce::Array<juce::File> PresetManager::userPresetFiles() const
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*" + presetExtension);
    files.sort();
    return files;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : factoryPresets())
        names.add (p.name);
    for (const auto& f : userPresetFiles())
        names.add (f.getFileNameWithoutExtension());
    return names;
}

Snapshot PresetManager::snapshot() const
{
    Snapshot values;
    for (auto* p : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            values.emplace_back (ranged->getParameterID().toStdString(), ranged->convertFrom0to1 (ranged->getValue()));
    return values;
}

void PresetManager::apply (const Snapshot& values, bool includeGlobal)
{
    for (const auto& [id, value] : values)
    {
        if (! includeGlobal && rowOf (id) == Row::global)
            continue;
        if (auto* p = state.getParameter (juce::String (id)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }
}

void PresetManager::resetToDefaults()
{
    for (auto* p : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rowOf (ranged->getParameterID().toStdString()) != Row::global)
            {
                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
                ranged->endChangeGesture();
            }
}

void PresetManager::loadPreset (int index)
{
    if (panelMode)
        return; // panel mode bypasses presets
    const auto names = getPresetNames();
    if (index < 0 || index >= names.size())
        return;

    const int numFactory = getNumFactoryPresets();
    if (index < numFactory)
    {
        resetToDefaults();
        apply (factoryPresets()[(size_t) index].values);
    }
    else
    {
        const auto file = userPresetFiles()[index - numFactory];
        if (auto xml = juce::XmlDocument::parse (file))
        {
            resetToDefaults();
            Snapshot values;
            for (auto* e : xml->getChildWithTagNameIterator ("Param"))
                values.emplace_back (e->getStringAttribute ("id").toStdString(), (float) e->getDoubleAttribute ("value"));
            apply (values);
        }
    }
    currentIndex = index;
    currentName = names[index];
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return false;

    juce::XmlElement xml ("RotorPreset");
    xml.setAttribute ("version", 1);
    for (const auto& [id, value] : snapshot())
    {
        if (rowOf (id) == Row::global)
            continue;
        auto* e = xml.createNewChildElement ("Param");
        e->setAttribute ("id", juce::String (id));
        e->setAttribute ("value", value);
    }
    if (! xml.writeTo (folder.getChildFile (clean + presetExtension)))
        return false;

    currentName = clean;
    currentIndex = getPresetNames().indexOf (clean);
    return true;
}

void PresetManager::storeSlot (Row row, int slot)
{
    const auto panel = snapshot();
    if (fullMode)
        bank.storeFull (slot, panel);
    else
        bank.store (row, slot, panel);
}

void PresetManager::recallSlot (Row row, int slot)
{
    if (panelMode)
        return; // panel mode bypasses presets
    apply (fullMode ? bank.recallFull (slot) : bank.recall (row, slot));
}

void PresetManager::randomizeRow (Row row)
{
    rotor::presets::Randomizer dice ((std::uint32_t) juce::Random::getSystemRandom().nextInt() | 1u);

    std::vector<std::string> ids;
    for (auto* p : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ids.push_back (ranged->getParameterID().toStdString());

    std::vector<std::string> fallbacks;
    auto values = dice.roll (row, ids, &fallbacks);
    // Any parameter without a rule: uniform across its own range.
    for (const auto& id : fallbacks)
        if (auto* p = state.getParameter (juce::String (id)))
            values.emplace_back (id, p->convertFrom0to1 ((float) dice.uniform()));
    apply (values);
}

void PresetManager::randomizeAll()
{
    for (int r = 0; r < rotor::presets::numRows; ++r)
        randomizeRow (static_cast<Row> (r));
}

juce::ValueTree PresetManager::toValueTree() const
{
    juce::ValueTree tree ("RowPresets");
    tree.setProperty ("fullMode", fullMode, nullptr);
    tree.setProperty ("panelMode", panelMode, nullptr);
    tree.setProperty ("presetName", currentName, nullptr);
    tree.setProperty ("presetIndex", currentIndex, nullptr);
    for (int r = 0; r < rotor::presets::numRows; ++r)
        for (int s = 0; s < rotor::presets::numSlots; ++s)
        {
            const auto row = static_cast<Row> (r);
            if (! bank.isFilled (row, s))
                continue;
            juce::ValueTree slot ("Slot");
            slot.setProperty ("row", r, nullptr);
            slot.setProperty ("slot", s, nullptr);
            for (const auto& [id, value] : bank.recall (row, s))
            {
                juce::ValueTree p ("Param");
                p.setProperty ("id", juce::String (id), nullptr);
                p.setProperty ("value", value, nullptr);
                slot.appendChild (p, nullptr);
            }
            tree.appendChild (slot, nullptr);
        }
    return tree;
}

void PresetManager::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType ("RowPresets"))
        return;
    fullMode = tree.getProperty ("fullMode", true);
    panelMode = tree.getProperty ("panelMode", false);
    currentName = tree.getProperty ("presetName", "Init").toString();
    currentIndex = tree.getProperty ("presetIndex", 0);
    bank = {};
    for (const auto& slot : tree)
    {
        Snapshot values;
        for (const auto& p : slot)
            values.emplace_back (p.getProperty ("id").toString().toStdString(), (float) (double) p.getProperty ("value"));
        bank.set (static_cast<Row> ((int) slot.getProperty ("row")), (int) slot.getProperty ("slot"), std::move (values));
    }
}
