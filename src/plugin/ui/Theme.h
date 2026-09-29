#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// "Synthwave, Toned Down" (mood board 03B): purple ground, dusty rose + muted teal,
// soft glow, faint horizon grid. Display type: Orbitron; labels: Share Tech Mono.
namespace rotor::ui
{

namespace colours
{
    inline const juce::Colour background { 0xff241c30 };
    inline const juce::Colour panel { 0xff2e2740 };
    inline const juce::Colour border { 0xff3d3552 };
    inline const juce::Colour rose { 0xffd98bc2 };
    inline const juce::Colour teal { 0xff8fc9d9 };
    inline const juce::Colour text { 0xffece6f0 };
    inline const juce::Colour track { 0xff1e1729 };
} // namespace colours

struct Fonts
{
    static juce::Typeface::Ptr display(); // Orbitron
    static juce::Typeface::Ptr mono();    // Share Tech Mono
    static juce::Font title (float height) { return juce::Font (juce::FontOptions (display()).withHeight (height)); }
    static juce::Font label (float height) { return juce::Font (juce::FontOptions (mono()).withHeight (height)); }
};

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override { return Fonts::mono(); }
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

// Small drawing helpers shared by the controls.
void drawGlow (juce::Graphics&, juce::Rectangle<float> area, juce::Colour, float radius, float alpha);

// The dice icon ("roll the dice" = randomize): a rounded die showing five pips.
void drawDice (juce::Graphics&, juce::Rectangle<float> area, bool highlighted);

} // namespace rotor::ui
