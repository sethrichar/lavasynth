#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/Theme.h"

// The Rotor panel: header (presets, FULL/ROW, options, master) and the hardware's three rows,
// each with its own 8 row-preset slots. Drawn at a fixed design size and scaled as a whole.
class RotorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RotorEditor (RotorAudioProcessor&);
    ~RotorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int designWidth = 1560;
    static constexpr int designHeight = 920;

private:
    class Panel;
    void timerCallback() override;

    RotorAudioProcessor& processor;
    rotor::ui::LookAndFeel lookAndFeel;
    std::unique_ptr<Panel> panel;
    juce::TooltipWindow tooltips { this, 600 };
};
