#include "Theme.h"

#include "BinaryData.h"

namespace rotor::ui
{

juce::Typeface::Ptr Fonts::display()
{
    static auto tf = juce::Typeface::createSystemTypefaceFor (BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
    return tf;
}

juce::Typeface::Ptr Fonts::mono()
{
    static auto tf = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf,
                                                              BinaryData::ShareTechMonoRegular_ttfSize);
    return tf;
}

LookAndFeel::LookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, rose.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::textColourOffId, rose);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::TextEditor::backgroundColourId, track);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, teal);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, border);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, border);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::panel);
    g.setColour (colours::border);
    g.drawRect (0, 0, width, height);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState() || down;
    g.setColour (on ? colours::rose.withAlpha (0.85f) : colours::track.withAlpha (highlighted ? 0.9f : 0.6f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (colours::rose.withAlpha (highlighted ? 1.0f : 0.8f));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
}

void drawGlow (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c, float radius, float alpha)
{
    // Soft glow: a few expanding, fading outlines (cheap, no blur).
    for (int i = 3; i >= 1; --i)
    {
        const float grow = radius * (float) i / 3.0f;
        g.setColour (c.withAlpha (alpha / (float) (i * 2)));
        g.drawRoundedRectangle (area.expanded (grow), area.getHeight() * 0.5f + grow, 1.5f);
    }
}

} // namespace rotor::ui
