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

void drawDice (juce::Graphics& g, juce::Rectangle<float> area, bool highlighted)
{
    // Button face, same style as a preset slot.
    g.setColour (highlighted ? colours::rose.withAlpha (0.25f) : colours::track);
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (colours::rose.withAlpha (highlighted ? 1.0f : 0.8f));
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    // The die: a small rounded square, tilted a touch as if mid-roll, with five pips.
    const float size = std::min (area.getWidth(), area.getHeight()) * 0.68f;
    const auto die = juce::Rectangle<float> (size, size).withCentre (area.getCentre());
    const auto rotate = juce::AffineTransform::rotation (-0.18f, die.getCentreX(), die.getCentreY());
    juce::Path body;
    body.addRoundedRectangle (die, size * 0.2f);
    g.setColour (colours::teal.withAlpha (highlighted ? 0.35f : 0.18f));
    g.fillPath (body, rotate);
    g.setColour (colours::teal);
    g.strokePath (body, juce::PathStrokeType (1.3f), rotate);

    const float pip = size * 0.16f, inset = size * 0.28f;
    const auto c = die.getCentre();
    g.setColour (colours::rose);
    for (auto offset : { juce::Point<float> (-inset, -inset), { inset, -inset }, { 0.0f, 0.0f }, { -inset, inset }, { inset, inset } })
    {
        const auto p = (c + offset).transformedBy (rotate);
        g.fillEllipse (juce::Rectangle<float> (pip, pip).withCentre (p));
    }
}

} // namespace rotor::ui
