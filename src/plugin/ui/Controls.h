#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace rotor::ui
{

// Base for controls bound to one parameter through a ParameterAttachment (host gestures,
// automation and undo all work). Holds the normalised value 0..1.
class ParameterControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParameterControl (juce::RangedAudioParameter& p, juce::String label);

protected:
    float getNormalised() const { return normalised; }
    void setNormalisedFromUi (float newNormalised, bool asGesture);
    juce::String valueText() const { return param.getCurrentValueAsText(); }
    void resetToDefault();
    void beginGesture() { attachment.beginGesture(); }
    void endGesture() { attachment.endGesture(); }

    juce::RangedAudioParameter& param;
    juce::String label;
    bool dragging = false;

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    float normalised = 0.0f;
    juce::ParameterAttachment attachment;
};

// Vertical fader. Bipolar faders fill from the centre; stepped faders snap and show ticks;
// inverted faders put the parameter's first value at the top (e.g. waveform: square at top).
class Fader final : public ParameterControl
{
public:
    enum class Style { unipolar, bipolar, stepped };
    Fader (juce::RangedAudioParameter& p, juce::String label, Style style, bool inverted = false);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { resetToDefault(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Optional labels drawn beside the ticks of a stepped fader (top to bottom).
    void setStepLabels (juce::StringArray labels) { stepLabels = std::move (labels); }

private:
    juce::Rectangle<float> trackArea() const;
    float displayPosition() const; // 0 = bottom, 1 = top
    void setFromDisplayPosition (float pos, bool asGesture);

    Style style;
    bool inverted;
    int numSteps = 0;
    float dragStartPos = 0.0f;
    int dragStartY = 0;
    juce::StringArray stepLabels;
};

// Rotary knob (used for key tracking, tune, master). Bipolar knobs light from 12 o'clock.
class Knob final : public ParameterControl
{
public:
    Knob (juce::RangedAudioParameter& p, juce::String label, bool bipolar);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { resetToDefault(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    bool bipolar;
    float dragStart = 0.0f;
    int dragStartY = 0;
};

// On/off button for a bool parameter.
class Toggle final : public ParameterControl
{
public:
    Toggle (juce::RangedAudioParameter& p, juce::String text);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
};

// Button for a choice parameter: shows the current choice; click = next, right-click = menu.
// (Used for Envelope Curve and Filter Character — the owner's "a button for each".)
class ChoiceButton final : public ParameterControl
{
public:
    ChoiceButton (juce::RangedAudioParameter& p, juce::String caption);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray choices;
};

// A row or column of cells for a small choice parameter (voice mode, LP/BP, slow/fast…).
class Segmented final : public ParameterControl
{
public:
    Segmented (juce::RangedAudioParameter& p, juce::StringArray cellLabels, bool vertical);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<float>) const;
    juce::StringArray cells;
    bool vertical;
};

} // namespace rotor::ui
