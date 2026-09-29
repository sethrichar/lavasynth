#include "Controls.h"

namespace rotor::ui
{

namespace
{
    constexpr float labelHeight = 14.0f;

    void drawLabel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float alpha = 0.7f)
    {
        g.setColour (colours::text.withAlpha (alpha));
        g.setFont (Fonts::label (11.0f));
        g.drawFittedText (text, r.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
} // namespace

// ---------------------------------------------------------------------------------------------

ParameterControl::ParameterControl (juce::RangedAudioParameter& p, juce::String labelText)
    : param (p), label (std::move (labelText)),
      attachment (p, [this] (float denormalised)
                  {
                      normalised = param.convertTo0to1 (denormalised);
                      repaint();
                  })
{
    setTooltip (p.getName (64));
    attachment.sendInitialUpdate();
}

void ParameterControl::setNormalisedFromUi (float newNormalised, bool asGesture)
{
    const float v = juce::jlimit (0.0f, 1.0f, newNormalised);
    const float denormalised = param.convertFrom0to1 (v);
    if (asGesture)
        attachment.setValueAsPartOfGesture (denormalised);
    else
        attachment.setValueAsCompleteGesture (denormalised);
}

void ParameterControl::resetToDefault()
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

// ---------------------------------------------------------------------------------------------

Fader::Fader (juce::RangedAudioParameter& p, juce::String labelText, Style s, bool inv)
    : ParameterControl (p, std::move (labelText)), style (s), inverted (inv)
{
    numSteps = style == Style::stepped ? juce::jmax (2, p.getNumSteps()) : 0;
}

juce::Rectangle<float> Fader::trackArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromBottom (labelHeight + 2.0f);
    r.removeFromTop (labelHeight);
    return r.reduced (0.0f, 8.0f);
}

float Fader::displayPosition() const { return inverted ? 1.0f - getNormalised() : getNormalised(); }

void Fader::setFromDisplayPosition (float pos, bool asGesture)
{
    pos = juce::jlimit (0.0f, 1.0f, pos);
    if (numSteps > 1)
        pos = std::round (pos * (float) (numSteps - 1)) / (float) (numSteps - 1);
    setNormalisedFromUi (inverted ? 1.0f - pos : pos, asGesture);
}

void Fader::paint (juce::Graphics& g)
{
    const auto area = trackArea();
    const float cx = area.getCentreX();
    const auto track = juce::Rectangle<float> (cx - 2.0f, area.getY(), 4.0f, area.getHeight());

    g.setColour (colours::track);
    g.fillRoundedRectangle (track.expanded (1.0f), 2.5f);

    const float pos = displayPosition();
    const float thumbY = area.getBottom() - pos * area.getHeight();

    // Fill: from the bottom, or from the centre for bipolar controls.
    const float from = style == Style::bipolar ? area.getCentreY() : area.getBottom();
    auto fill = juce::Rectangle<float>::leftTopRightBottom (track.getX(), std::min (from, thumbY), track.getRight(), std::max (from, thumbY));
    if (style != Style::stepped)
    {
        g.setGradientFill (juce::ColourGradient (colours::teal, cx, area.getBottom(), colours::rose, cx, area.getY(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    }

    // Ticks for stepped faders (and the centre detent for bipolar).
    g.setColour (colours::border.brighter (0.4f));
    if (numSteps > 1)
    {
        for (int i = 0; i < numSteps; ++i)
        {
            const float y = area.getBottom() - (float) i / (float) (numSteps - 1) * area.getHeight();
            g.fillRect (cx - 9.0f, y - 0.5f, 18.0f, 1.0f);
        }
        if (! stepLabels.isEmpty())
        {
            g.setFont (Fonts::label (8.5f));
            for (int i = 0; i < stepLabels.size() && i < numSteps; ++i)
            {
                const float y = area.getY() + (float) i / (float) (numSteps - 1) * area.getHeight();
                g.setColour (colours::text.withAlpha (0.45f));
                g.drawText (stepLabels[i], juce::Rectangle<float> (cx + 11.0f, y - 5.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
            }
        }
    }
    else if (style == Style::bipolar)
    {
        g.fillRect (cx - 8.0f, area.getCentreY() - 0.5f, 16.0f, 1.0f);
    }

    // Thumb: dusty-rose cap with a soft glow.
    const auto thumb = juce::Rectangle<float> (22.0f, 9.0f).withCentre ({ cx, thumbY });
    const bool active = dragging || isMouseOver();
    drawGlow (g, thumb, colours::rose, active ? 5.0f : 3.0f, active ? 0.35f : 0.18f);
    g.setColour (colours::rose);
    g.fillRoundedRectangle (thumb, 2.5f);
    g.setColour (colours::background.withAlpha (0.6f));
    g.fillRect (thumb.getX() + 4.0f, thumbY - 0.5f, thumb.getWidth() - 8.0f, 1.0f);

    // Top: the value while touched; bottom: the label.
    if (active)
    {
        g.setColour (colours::teal);
        g.setFont (Fonts::label (10.0f));
        g.drawFittedText (valueText(), getLocalBounds().removeFromTop ((int) labelHeight), juce::Justification::centred, 1, 0.7f);
    }
    drawLabel (g, getLocalBounds().toFloat().removeFromBottom (labelHeight), label);
}

void Fader::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    dragging = true;
    dragStartPos = displayPosition();
    dragStartY = e.y;
    beginGesture();
    // Clicking on the track away from the thumb jumps there (except fine-adjust with shift).
    const auto area = trackArea();
    const float thumbY = area.getBottom() - dragStartPos * area.getHeight();
    if (! e.mods.isShiftDown() && std::abs ((float) e.y - thumbY) > 10.0f && area.contains (e.position))
    {
        dragStartPos = (area.getBottom() - (float) e.y) / area.getHeight();
        setFromDisplayPosition (dragStartPos, true);
    }
    repaint();
}

void Fader::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    const float sensitivity = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float delta = (float) (dragStartY - e.y) / trackArea().getHeight() * sensitivity;
    setFromDisplayPosition (dragStartPos + delta, true);
}

void Fader::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        endGesture();
    dragging = false;
    repaint();
}

void Fader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float step = numSteps > 1 ? 1.0f / (float) (numSteps - 1) * (w.deltaY > 0 ? 1.0f : -1.0f) : w.deltaY * 0.1f;
    setFromDisplayPosition (displayPosition() + step, false);
}

// ---------------------------------------------------------------------------------------------

Knob::Knob (juce::RangedAudioParameter& p, juce::String labelText, bool isBipolar)
    : ParameterControl (p, std::move (labelText)), bipolar (isBipolar) {}

void Knob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto labelArea = bounds.removeFromBottom (labelHeight);
    const float size = std::min (bounds.getWidth(), bounds.getHeight()) - 10.0f;
    const auto circle = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const bool active = dragging || isMouseOver();

    const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
    const float angle = start + getNormalised() * (end - start);

    // Value arc.
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path arc;
    arc.addCentredArc (circle.getCentreX(), circle.getCentreY(), size * 0.5f + 4.0f, size * 0.5f + 4.0f, 0.0f,
                       std::min (from, angle), std::max (from, angle), true);
    g.setColour (colours::rose.withAlpha (0.8f));
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Body: panel fill, teal ring, soft glow, rose pointer (mood board 03B).
    drawGlow (g, circle, colours::teal, active ? 5.0f : 3.0f, active ? 0.3f : 0.15f);
    g.setColour (colours::panel);
    g.fillEllipse (circle);
    g.setColour (colours::teal);
    g.drawEllipse (circle, 2.0f);
    const auto c = circle.getCentre();
    const auto tip = c.getPointOnCircumference (size * 0.5f - 4.0f, angle);
    const auto base = c.getPointOnCircumference (size * 0.12f, angle);
    g.setColour (colours::rose);
    g.drawLine ({ base, tip }, 2.0f);

    if (active)
    {
        g.setColour (colours::teal);
        g.setFont (Fonts::label (10.0f));
        g.drawFittedText (valueText(), labelArea.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
    else
    {
        drawLabel (g, labelArea, label);
    }
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    dragStart = getNormalised();
    dragStartY = e.y;
    beginGesture();
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const float sensitivity = e.mods.isShiftDown() ? 0.001f : 0.005f;
    setNormalisedFromUi (dragStart + (float) (dragStartY - e.y) * sensitivity, true);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        endGesture();
    dragging = false;
    repaint();
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    setNormalisedFromUi (getNormalised() + w.deltaY * 0.05f, false);
}

// ---------------------------------------------------------------------------------------------

Toggle::Toggle (juce::RangedAudioParameter& p, juce::String text) : ParameterControl (p, std::move (text)) {}

void Toggle::paint (juce::Graphics& g)
{
    const bool on = getNormalised() >= 0.5f;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (on)
        drawGlow (g, r, colours::rose, 3.0f, 0.25f);
    g.setColour (on ? colours::rose : colours::track);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (colours::rose.withAlpha (isMouseOver() ? 1.0f : 0.75f));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setColour (on ? colours::background : colours::rose);
    g.setFont (Fonts::label (10.5f));
    // A toggle captioned "ON" (the option switches) reads ON / OFF.
    const auto text = label == "ON" ? juce::String (on ? "ON" : "OFF") : label;
    g.drawFittedText (text, getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
}

void Toggle::mouseDown (const juce::MouseEvent&)
{
    setNormalisedFromUi (getNormalised() >= 0.5f ? 0.0f : 1.0f, false);
}

// ---------------------------------------------------------------------------------------------

ChoiceButton::ChoiceButton (juce::RangedAudioParameter& p, juce::String caption) : ParameterControl (p, std::move (caption))
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
        choices = choice->choices;
}

void ChoiceButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto caption = r.removeFromTop (labelHeight);
    drawLabel (g, caption, label, 0.6f);
    r = r.reduced (1.0f);
    g.setColour (colours::track);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (colours::rose.withAlpha (isMouseOver() ? 1.0f : 0.75f));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setColour (colours::rose);
    g.setFont (Fonts::label (11.0f));
    g.drawFittedText (valueText().toUpperCase(), r.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.6f);
}

void ChoiceButton::mouseDown (const juce::MouseEvent& e)
{
    const int n = std::max (1, choices.size());
    const int current = juce::roundToInt (getNormalised() * (float) (n - 1));
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        for (int i = 0; i < n; ++i)
            menu.addItem (i + 1, choices[i], true, i == current);
        juce::Component::SafePointer<ChoiceButton> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, n] (int result)
                            {
                                if (safe != nullptr && result > 0)
                                    safe->setNormalisedFromUi ((float) (result - 1) / (float) (n - 1), false);
                            });
        return;
    }
    setNormalisedFromUi ((float) ((current + 1) % n) / (float) (n - 1), false);
}

// ---------------------------------------------------------------------------------------------

Segmented::Segmented (juce::RangedAudioParameter& p, juce::StringArray cellLabels, bool isVertical)
    : ParameterControl (p, {}), cells (std::move (cellLabels)), vertical (isVertical) {}

int Segmented::indexAt (juce::Point<float> pt) const
{
    const int n = std::max (1, cells.size());
    const float f = vertical ? pt.y / (float) getHeight() : pt.x / (float) getWidth();
    return juce::jlimit (0, n - 1, (int) (f * (float) n));
}

void Segmented::paint (juce::Graphics& g)
{
    const int n = std::max (1, cells.size());
    const int current = juce::roundToInt (getNormalised() * (float) (n - 1));
    auto r = getLocalBounds().toFloat();
    for (int i = 0; i < n; ++i)
    {
        auto cell = vertical ? juce::Rectangle<float> (r.getX(), r.getY() + r.getHeight() * (float) i / (float) n, r.getWidth(), r.getHeight() / (float) n)
                             : juce::Rectangle<float> (r.getX() + r.getWidth() * (float) i / (float) n, r.getY(), r.getWidth() / (float) n, r.getHeight());
        cell = cell.reduced (1.0f);
        const bool on = i == current;
        g.setColour (on ? colours::rose : colours::track);
        g.fillRoundedRectangle (cell, 2.0f);
        g.setColour (colours::rose.withAlpha (on ? 1.0f : 0.45f));
        g.drawRoundedRectangle (cell, 2.0f, 1.0f);
        g.setColour (on ? colours::background : colours::text.withAlpha (0.75f));
        g.setFont (Fonts::label (10.0f));
        g.drawFittedText (cells[i], cell.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.6f);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    const int n = std::max (2, cells.size());
    setNormalisedFromUi ((float) indexAt (e.position) / (float) (n - 1), false);
}

} // namespace rotor::ui
