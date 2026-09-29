#include "PluginEditor.h"

using namespace rotor::ui;
using rotor::presets::Row;

namespace
{
    constexpr int margin = 10;
    constexpr int headerHeight = 70;
    constexpr int rowHeight = 270;
    constexpr int slotStripWidth = 86;
    constexpr int faderWidth = 46;
    constexpr int sectionTitle = 22;
    constexpr int sectionGap = 10;
} // namespace

// A named panel section drawn behind its controls.
struct Section
{
    juce::String title;
    juce::Rectangle<int> bounds;
};

// ---------------------------------------------------------------------------------------------
// The row-preset strip at the left of each row: 8 slots + STORE.
class SlotStrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SlotStrip (PresetManager& pm, Row r, juce::String number) : presets (pm), row (r), rowNumber (std::move (number))
    {
        setTooltip ("Row " + rowNumber + ": click a slot to recall, STORE then a slot to save, dice to randomize this row");
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        g.setFont (Fonts::title (26.0f));
        g.setColour (colours::teal.withAlpha (0.8f));
        g.drawText (rowNumber, getLocalBounds().removeFromTop (40), juce::Justification::centred);

        // In panel mode the slots can't recall, so they're dimmed (they still accept STORE).
        const bool dimmed = presets.isPanelMode() && ! storeArmed;
        if (dimmed)
            g.beginTransparencyLayer (0.4f);
        for (int i = 0; i < rotor::presets::numSlots; ++i)
        {
            const auto cell = slotBounds (i).toFloat();
            const bool filled = presets.isSlotFilled (row, i);
            const bool hot = hover == i && ! dimmed;
            g.setColour (hot ? colours::rose.withAlpha (0.25f) : colours::track);
            g.fillRoundedRectangle (cell, 3.0f);
            g.setColour ((storeArmed ? colours::rose : colours::teal).withAlpha (filled || hot ? 0.9f : 0.35f));
            g.drawRoundedRectangle (cell, 3.0f, 1.0f);
            g.setColour (colours::text.withAlpha (filled ? 0.95f : 0.45f));
            g.setFont (Fonts::label (12.0f));
            g.drawText (juce::String (i + 1), cell, juce::Justification::centred);
            if (filled)
            {
                g.setColour (colours::teal);
                g.fillEllipse (cell.getRight() - 7.0f, cell.getY() + 3.0f, 4.0f, 4.0f);
            }
        }
        if (dimmed)
            g.endTransparencyLayer();

        drawDice (g, diceBounds().toFloat(), hover == diceIndex);

        const auto store = storeBounds().toFloat();
        g.setColour (storeArmed ? colours::rose : colours::track);
        g.fillRoundedRectangle (store, 3.0f);
        g.setColour (colours::rose);
        g.drawRoundedRectangle (store, 3.0f, 1.0f);
        g.setColour (storeArmed ? colours::background : colours::rose);
        g.setFont (Fonts::label (11.0f));
        g.drawText (storeArmed ? "PICK SLOT" : "STORE", store, juce::Justification::centred);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = diceBounds().contains (e.getPosition()) ? diceIndex : slotAt (e.getPosition());
        if (h != hover) { hover = h; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (storeBounds().contains (e.getPosition()))
        {
            storeArmed = ! storeArmed;
            repaint();
            return;
        }
        if (diceBounds().contains (e.getPosition()))
        {
            presets.randomizeRow (row); // roll the dice for this row
            return;
        }
        const int slot = slotAt (e.getPosition());
        if (slot < 0)
            return;
        if (storeArmed)
        {
            presets.storeSlot (row, slot);
            storeArmed = false;
        }
        else
        {
            presets.recallSlot (row, slot);
        }
        if (auto* parent = getParentComponent())
            parent->repaint(); // FULL mode may have filled the other rows too
    }

private:
    juce::Rectangle<int> slotBounds (int i) const
    {
        const int w = (getWidth() - 18) / 2, h = 30;
        return { 6 + (i % 2) * (w + 6), 44 + (i / 2) * (h + 6), w, h };
    }
    juce::Rectangle<int> storeBounds() const { return { 6, getHeight() - 34, getWidth() - 12, 26 }; }
    // Same size as a slot, centred under the eight slots, above STORE.
    juce::Rectangle<int> diceBounds() const
    {
        const auto slot = slotBounds (0);
        return slot.withPosition ((getWidth() - slot.getWidth()) / 2, slotBounds (7).getBottom() + 10);
    }
    static constexpr int diceIndex = 100;
    int slotAt (juce::Point<int> p) const
    {
        for (int i = 0; i < rotor::presets::numSlots; ++i)
            if (slotBounds (i).contains (p))
                return i;
        return -1;
    }

    PresetManager& presets;
    Row row;
    juce::String rowNumber;
    bool storeArmed = false;
    int hover = -1;
};

// ---------------------------------------------------------------------------------------------
// A toggle bound to plugin state that isn't a parameter (e.g. PANEL mode).
class StateToggle final : public juce::Component, public juce::SettableTooltipClient
{
public:
    StateToggle (juce::String text, std::function<bool()> get, std::function<void (bool)> set)
        : label (std::move (text)), getter (std::move (get)), setter (std::move (set)) {}

    void paint (juce::Graphics& g) override
    {
        const bool on = getter();
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if (on)
            drawGlow (g, r, colours::rose, 3.0f, 0.25f);
        g.setColour (on ? colours::rose : colours::track);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (colours::rose.withAlpha (isMouseOver() ? 1.0f : 0.75f));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
        g.setColour (on ? colours::background : colours::rose);
        g.setFont (Fonts::label (10.5f));
        g.drawFittedText (label, getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        setter (! getter());
        if (auto* parent = getParentComponent())
            parent->repaint();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::String label;
    std::function<bool()> getter;
    std::function<void (bool)> setter;
};

// The dice button (same look as the row dice).
class DiceButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DiceButton (std::function<void()> roll) : onRoll (std::move (roll)) {}
    void paint (juce::Graphics& g) override { drawDice (g, getLocalBounds().toFloat().reduced (0.5f), isMouseOver()); }
    void mouseDown (const juce::MouseEvent&) override { onRoll(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    std::function<void()> onRoll;
};

// ---------------------------------------------------------------------------------------------
// Everything, laid out at the design size.
class RotorEditor::Panel final : public juce::Component
{
public:
    Panel (RotorAudioProcessor& p) : processor (p), presets (p.getPresetManager())
    {
        buildHeader();
        buildRow1();
        buildRow2();
        buildRow3();
        buildOptions();
        setSize (designWidth, designHeight);
    }

    void refresh()
    {
        presetName.setButtonText (presets.getCurrentName());
        fullRow.setButtonText (presets.isFullMode() ? "FULL" : "ROW");
        fullRow.setToggleState (presets.isFullMode(), juce::dontSendNotification);
        const bool bypassed = presets.isPanelMode();
        for (auto* b : { &prev, &next, &presetName })
            b->setAlpha (bypassed ? 0.4f : 1.0f);
        presetName.setTooltip (bypassed ? "PANEL mode is on: presets are bypassed" : "Choose a preset");
        for (auto* s : slotStrips)
            s->repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        // Ground: purple gradient, faint horizon grid, a low rose glow at the base.
        g.setGradientFill (juce::ColourGradient (colours::panel, 0.0f, 0.0f, colours::background, r.getWidth() * 0.35f, r.getHeight(), false));
        g.fillAll();
        g.setColour (juce::Colours::white.withAlpha (0.024f));
        for (float y = 26.0f; y < r.getHeight(); y += 26.0f)
            g.fillRect (0.0f, y, r.getWidth(), 1.0f);
        g.setGradientFill (juce::ColourGradient (colours::rose.withAlpha (0.0f), 0.0f, r.getHeight() * 0.85f,
                                                 colours::rose.withAlpha (0.09f), 0.0f, r.getHeight(), false));
        g.fillRect (r.withTop (r.getHeight() * 0.85f));

        // Title: Orbitron, rose → teal gradient (drawn as a path so the gradient spans the word,
        // stroked lightly to give the thin face more body).
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (Fonts::title (44.0f), "ROTOR", (float) margin + 6.0f, 52.0f);
            juce::Path title;
            ga.createPath (title);
            const auto tb = title.getBounds();
            g.setColour (colours::rose.withAlpha (0.12f));
            g.strokePath (title, juce::PathStrokeType (5.0f));
            g.setGradientFill (juce::ColourGradient (colours::rose, tb.getX(), 0.0f, colours::teal, tb.getRight(), 0.0f, false));
            g.fillPath (title);
            g.strokePath (title, juce::PathStrokeType (1.2f));
            g.setColour (colours::text.withAlpha (0.45f));
            g.setFont (Fonts::label (11.0f));
            g.drawText ("5-VOICE ROTATING POLYSYNTH", juce::Rectangle<float> (tb.getRight() + 16.0f, tb.getBottom() - 14.0f, 240.0f, 14.0f),
                        juce::Justification::centredLeft);
        }

        for (const auto& s : sections)
        {
            const auto b = s.bounds.toFloat();
            g.setColour (colours::panel.withAlpha (0.92f));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (colours::border);
            g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
            g.setColour (colours::teal.withAlpha (0.85f));
            g.setFont (Fonts::label (12.5f));
            g.drawText (s.title, b.reduced (12.0f, 0.0f).removeFromTop ((float) sectionTitle + 4.0f), juce::Justification::centredLeft);
        }

        // Small captions over the section panels.
        g.setColour (colours::text.withAlpha (0.55f));
        g.setFont (Fonts::label (10.5f));
        for (const auto& [text, area] : captions)
            g.drawText (text, area, juce::Justification::centred);
    }

    void resized() override {} // fixed design layout (placed while building)

private:
    // ---- helpers --------------------------------------------------------------------------
    juce::RangedAudioParameter& param (const juce::String& id) { return *processor.getState().getParameter (id); }

    template <typename T, typename... Args>
    T& add (juce::Rectangle<int> bounds, Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        addAndMakeVisible (ref);
        ref.setBounds (bounds);
        owned.push_back (std::move (c));
        return ref;
    }

    Fader& fader (int x, int y, const juce::String& id, const juce::String& label, Fader::Style style = Fader::Style::unipolar,
                  bool inverted = false, int height = -1)
    {
        return add<Fader> ({ x, y, faderWidth, height > 0 ? height : faderHeight }, param (id), label, style, inverted);
    }

    Section& section (const juce::String& title, int x, int y, int width, int height = rowHeight)
    {
        sections.push_back ({ title, { x, y, width, height } });
        return sections.back();
    }

    int rowY (int row) const { return headerHeight + row * (rowHeight + margin); }
    int contentX() const { return margin + slotStripWidth + sectionGap; }

    void addSlotStrip (Row row, int index)
    {
        auto strip = std::make_unique<SlotStrip> (presets, row, juce::String (index + 1));
        addAndMakeVisible (*strip);
        strip->setBounds (margin, rowY (index), slotStripWidth, rowHeight);
        slotStrips.add (strip.get());
        owned.push_back (std::move (strip));
    }

    // ---- header ----------------------------------------------------------------------------
    void buildHeader()
    {
        const int y = 18, h = 30;
        int x = 520;
        for (auto* b : { &prev, &presetName, &next, &save, &fullRow, &optionsButton })
            addAndMakeVisible (*b);
        prev.setButtonText ("<");
        next.setButtonText (">");
        save.setButtonText ("SAVE");
        optionsButton.setButtonText ("OPTIONS");
        prev.setBounds (x, y, 30, h);
        presetName.setBounds (x + 34, y, 260, h);
        next.setBounds (x + 298, y, 30, h);
        save.setBounds (x + 336, y, 64, h);
        fullRow.setBounds (x + 420, y, 64, h);
        fullRow.setTooltip ("Row preset slots: FULL recalls/stores all three rows, ROW just that row");
        optionsButton.setBounds (x + 504, y, 90, h);

        prev.onClick = [this] { presets.loadPrevious(); refresh(); };
        next.onClick = [this] { presets.loadNext(); refresh(); };
        presetName.onClick = [this] { showPresetMenu(); };
        save.onClick = [this] { askPresetName(); };
        fullRow.onClick = [this] { presets.setFullMode (! presets.isFullMode()); refresh(); };
        optionsButton.onClick = [this] { options->setVisible (! options->isVisible()); options->toFront (false); };

        add<Knob> ({ designWidth - margin - 70, 4, 64, 62 }, param ("level"), "MASTER", false);
        refresh();
    }

    void showPresetMenu()
    {
        juce::PopupMenu menu;
        const auto names = presets.getPresetNames();
        const int factory = presets.getNumFactoryPresets();
        menu.addSectionHeader ("FACTORY");
        for (int i = 0; i < names.size(); ++i)
        {
            if (i == factory)
                menu.addSectionHeader ("USER");
            menu.addItem (i + 1, names[i], true, i == presets.getCurrentIndex());
        }
        menu.addSeparator();
        menu.addItem (-1, "Open presets folder");
        juce::Component::SafePointer<Panel> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName), [safe] (int result)
                            {
                                if (safe == nullptr) return;
                                if (result == -1)
                                {
                                    auto folder = PresetManager::getUserPresetFolder();
                                    folder.createDirectory();
                                    folder.revealToUser();
                                }
                                else if (result > 0)
                                {
                                    safe->presets.loadPreset (result - 1);
                                    safe->refresh();
                                }
                            });
    }

    void askPresetName()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Name for this preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", presets.getCurrentName());
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<Panel> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
                            {
                                if (safe != nullptr && result == 1)
                                {
                                    safe->presets.saveUserPreset (w->getTextEditorContents ("name"));
                                    safe->refresh();
                                }
                            }), true);
    }

    // ---- row 1: voices + global --------------------------------------------------------------
    void buildRow1()
    {
        addSlotStrip (Row::voices, 0);
        const int y = rowY (0), top = y + sectionTitle + 8;
        int x = contentX();

        // VOICES: mode + 5 voice strips (on, level, octave, waveform).
        constexpr int voicesWidth = 96 + 5 * (3 * faderWidth + 3) + 12;
        section ("VOICES", x, y, voicesWidth);
        add<Segmented> ({ x + 10, top + 4, 74, 196 }, param ("voiceMode"),
                        juce::StringArray { "FWD", "BWD", "RND", "STACC", "LEGATO", "MONO" }, true);
        int vx = x + 96;
        for (int v = 1; v <= 5; ++v)
        {
            const auto n = juce::String (v);
            add<Toggle> ({ vx + 4, top, 3 * faderWidth - 8, 22 }, param ("voice" + n + "On"), "VOICE " + n);
            fader (vx, top + 26, "voice" + n + "Level", "LVL", Fader::Style::unipolar, false, faderHeight - 26);
            fader (vx + faderWidth, top + 26, "voice" + n + "Octave", "OCT", Fader::Style::stepped, false, faderHeight - 26);
            fader (vx + 2 * faderWidth, top + 26, "voice" + n + "Waveform", "WAVE", Fader::Style::stepped, true, faderHeight - 26);
            vx += 3 * faderWidth + 3;
        }
        x += voicesWidth + sectionGap;

        // GLOBAL: OSC level, sub, noise/EXT + colour, glide, phase distortion offset.
        const int w = designWidth - margin - x;
        section ("GLOBAL", x, y, w);
        // Faders are shortened to leave a line of buttons under SUB and NOISE.
        const int shortFader = faderHeight - 30, buttonsY = top + shortFader + 4;
        int gx = x + 12;
        fader (gx, top, "oscLevel", "OSC", Fader::Style::unipolar, false, shortFader); gx += faderWidth + 16;
        fader (gx, top, "subLevel", "SUB", Fader::Style::unipolar, false, shortFader);
        add<Segmented> ({ gx - 6, buttonsY, 50, 22 }, param ("subOctave"), juce::StringArray { "-1", "-2" }, false);
        add<Segmented> ({ gx + 48, buttonsY, 60, 22 }, param ("subWaveform"), juce::StringArray { "SIN", "SQR" }, false);
        gx += faderWidth + 70;
        fader (gx, top, "noiseLevel", "NOISE", Fader::Style::unipolar, false, shortFader); gx += faderWidth;
        fader (gx, top, "noiseColor", "COLOR", Fader::Style::bipolar, false, shortFader);
        add<Toggle> ({ gx - faderWidth + 6, buttonsY, 2 * faderWidth - 12, 22 }, param ("extInput"), "EXT");
        gx += faderWidth + 24;
        fader (gx, top, "glide", "GLIDE", Fader::Style::unipolar, false, shortFader); gx += faderWidth;
        fader (gx, top, "phaseDist", "PD", Fader::Style::bipolar, false, shortFader);

        // Right-hand column: clock sync, panel mode, and the global dice.
        const int colX = x + w - 214, colW = 196;
        captions.push_back ({ "CLOCK SYNC", { colX, top - 2, colW, 14 } });
        add<Toggle> ({ colX, top + 14, colW / 2 - 4, 26 }, param ("envSync"), "ENV SYNC").setTooltip ("Envelope times snap to note values at the host tempo");
        add<Toggle> ({ colX + colW / 2 + 4, top + 14, colW / 2 - 4, 26 }, param ("lfoSync"), "LFO SYNC").setTooltip ("LFO rate snaps to note values at the host tempo");
        captions.push_back ({ "PANEL MODE", { colX, top + 52, colW, 14 } });
        add<StateToggle> ({ colX, top + 68, colW, 26 }, "PANEL",
                          [this] { return presets.isPanelMode(); },
                          [this] (bool on) { presets.setPanelMode (on); refresh(); })
            .setTooltip ("Panel mode: presets and row slots are bypassed — the panel sounds exactly as it is");
        captions.push_back ({ "RANDOM ALL", { colX, top + 106, colW, 14 } });
        const auto slotSize = juce::Rectangle<int> (34, 30); // same size as a row preset slot
        add<DiceButton> (slotSize.withPosition (colX + (colW - slotSize.getWidth()) / 2, top + 122),
                         [this] { presets.randomizeAll(); })
            .setTooltip ("Roll the dice: randomize all three rows (global options stay put)");
    }

    // ---- row 2: aftertouch, amp env, mod env, filter ------------------------------------------
    void buildRow2()
    {
        addSlotStrip (Row::shaping, 1);
        const int y = rowY (1), top = y + sectionTitle + 8;
        int x = contentX();

        section ("AFTERTOUCH", x, y, 3 * faderWidth + 20);
        fader (x + 10, top, "atWildcard", "WILD", Fader::Style::bipolar);
        fader (x + 10 + faderWidth, top, "atCutoff", "CUT", Fader::Style::bipolar);
        fader (x + 10 + 2 * faderWidth, top, "atLfoRate", "LFO", Fader::Style::bipolar);
        x += 3 * faderWidth + 20 + sectionGap;

        const int envWidth = 4 * faderWidth + 110;
        section ("AMP ENVELOPE", x, y, envWidth);
        buildEnvelope (x, top, "attack", "decay", "sustain", "release", "ampLoop", "ampKeyTrack");
        add<ChoiceButton> ({ x + 4 * faderWidth + 16, top + 150, 88, 40 }, param ("envCurve"), "CURVE");
        x += envWidth + sectionGap;

        const int modWidth = 4 * faderWidth + 110 + 5 * faderWidth + 10;
        section ("MOD ENVELOPE", x, y, modWidth);
        buildEnvelope (x, top, "modAttack", "modDecay", "modSustain", "modRelease", "modLoop", "modKeyTrack");
        int mx = x + 4 * faderWidth + 110;
        for (auto [id, label] : { std::pair { "modEnvToPd", "PD" }, { "modEnvToCutoff", "CUT" }, { "modEnvToLfoRate", "LFO" },
                                  { "modEnvToSpread", "SPRD" }, { "modEnvToFold", "FOLD" } })
        {
            fader (mx, top, id, label, Fader::Style::bipolar);
            mx += faderWidth;
        }
        x += modWidth + sectionGap;

        section ("FILTER", x, y, designWidth - margin - x);
        fader (x + 10, top, "cutoff", "CUTOFF");
        fader (x + 10 + faderWidth, top, "resonance", "RES");
        const int cx = x + 20 + 2 * faderWidth;
        add<Segmented> ({ cx, top, 96, 24 }, param ("filterMode"), juce::StringArray { "LP 24", "BP" }, false);
        add<Knob> ({ cx + 14, top + 34, 68, 72 }, param ("keyTrack"), "KEY TRK", false);
        add<ChoiceButton> ({ cx - 2, top + 150, 100, 40 }, param ("filterCharacter"), "CHARACTER");
    }

    void buildEnvelope (int x, int top, const char* a, const char* d, const char* s, const char* r, const char* loop, const char* kt)
    {
        fader (x + 10, top, a, "A");
        fader (x + 10 + faderWidth, top, d, "D");
        fader (x + 10 + 2 * faderWidth, top, s, "S");
        fader (x + 10 + 3 * faderWidth, top, r, "R");
        const int cx = x + 4 * faderWidth + 16;
        add<Toggle> ({ cx, top, 88, 24 }, param (loop), "LOOP");
        add<Knob> ({ cx + 10, top + 34, 68, 72 }, param (kt), "KEY TRK", true);
    }

    // ---- row 3: LFO, effects, wildcards --------------------------------------------------------
    void buildRow3()
    {
        addSlotStrip (Row::motion, 2);
        const int y = rowY (2), top = y + sectionTitle + 8;
        int x = contentX();

        const int lfoWidth = 2 * faderWidth + 44 + 104 + 8 * faderWidth + 12;
        section ("LFO", x, y, lfoWidth);
        fader (x + 10, top, "lfoRate", "RATE");
        auto& shape = fader (x + 10 + faderWidth, top, "lfoShape", "SHAPE", Fader::Style::stepped, true);
        shape.setSize (faderWidth + 30, faderHeight);
        shape.setStepLabels ({ "VOLC", "SQR", "RSAW", "SAW", "SIN" });
        const int cx = x + 2 * faderWidth + 44;
        add<Segmented> ({ cx, top, 92, 24 }, param ("lfoRange"), juce::StringArray { "SLOW", "FAST" }, false);
        add<Knob> ({ cx + 12, top + 32, 68, 72 }, param ("lfoKeyTrack"), "KEY TRK", true);
        add<Toggle> ({ cx, top + 112, 92, 24 }, param ("lfoRetrigger"), "RETRIG");
        int lx = cx + 104;
        for (auto [id, label] : { std::pair { "lfoToPd", "PD" }, { "lfoToCutoff", "CUT" }, { "lfoToSpread", "SPRD" },
                                  { "lfoToFold", "FOLD" }, { "lfoToReverbMix", "RV MIX" }, { "lfoToDriveMix", "DR MIX" },
                                  { "lfoToFxAmount", "FX AMT" }, { "lfoToFxColor", "FX COL" } })
        {
            fader (lx, top, id, label, Fader::Style::bipolar);
            lx += faderWidth;
        }
        x += lfoWidth + sectionGap;

        const int fxWidth = 5 * faderWidth + 20;
        section ("EFFECTS", x, y, fxWidth);
        int fx = x + 10;
        for (auto [id, label] : { std::pair { "reverbAmount", "RV AMT" }, { "reverbMix", "RV MIX" },
                                  { "driveAmount", "DR AMT" }, { "driveMix", "DR MIX" } })
        {
            fader (fx, top, id, label);
            fx += faderWidth;
        }
        fader (fx, top, "fxColor", "COLOR", Fader::Style::bipolar);
        x += fxWidth + sectionGap;

        section ("WILDCARDS", x, y, designWidth - margin - x);
        int wx = x + 10;
        for (auto [id, label] : { std::pair { "noteDetune", "DETUNE" }, { "wow", "WOW" }, { "flutter", "FLUTTR" },
                                  { "reelDrag", "DRAG" }, { "chaos", "CHAOS" }, { "envScatter", "SCATTR" },
                                  { "spread", "SPREAD" }, { "fold", "FOLD" } })
        {
            fader (wx, top, id, label);
            wx += faderWidth + 2;
        }
    }

    // ---- options overlay (the hardware's DIP switches) ---------------------------------------
    class OptionsPanel final : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            drawGlow (g, r.reduced (4.0f), colours::teal, 6.0f, 0.2f);
            g.setColour (colours::panel);
            g.fillRoundedRectangle (r.reduced (2.0f), 10.0f);
            g.setColour (colours::teal.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (2.5f), 10.0f, 1.0f);
            g.setColour (colours::teal);
            g.setFont (Fonts::label (13.0f));
            g.drawText ("OPTIONS", getLocalBounds().reduced (16, 10).removeFromTop (20), juce::Justification::centredLeft);
            g.setColour (colours::text.withAlpha (0.6f));
            g.setFont (Fonts::label (10.5f));
            for (const auto& [text, y] : captions)
                g.drawText (text, 16, y, 150, 18, juce::Justification::centredLeft);
        }
        std::vector<std::pair<juce::String, int>> captions;
    };

    void buildOptions()
    {
        options = std::make_unique<OptionsPanel>();
        addChildComponent (*options);
        options->setBounds (designWidth - margin - 420, headerHeight - 12, 420, 338);
        auto place = [this] (std::unique_ptr<juce::Component> c, juce::Rectangle<int> b)
        {
            options->addAndMakeVisible (*c);
            c->setBounds (b);
            owned.push_back (std::move (c));
        };
        int y = 40;
        auto row = [&] (const juce::String& caption)
        {
            options->captions.push_back ({ caption, y + 3 });
            const int at = y;
            y += 32;
            return at;
        };
        place (std::make_unique<Toggle> (param ("roundRobinReset"), "ON"), { 180, row ("ROUND-ROBIN RESET"), 70, 24 });
        place (std::make_unique<Toggle> (param ("unisonGrace"), "ON"), { 180, row ("UNISON GRACE"), 70, 24 });
        place (std::make_unique<Segmented> (param ("monoPriority"), juce::StringArray { "LAST", "LOW", "HIGH" }, false),
               { 180, row ("MONO PRIORITY"), 220, 24 });
        place (std::make_unique<Toggle> (param ("mpe"), "ON"), { 180, row ("MPE"), 70, 24 });
        place (std::make_unique<Segmented> (param ("modWheelMode"), juce::StringArray { "WILDCARDS", "PITCH LFO" }, false),
               { 180, row ("MOD WHEEL"), 220, 24 });
        place (std::make_unique<Segmented> (param ("tuneMode"), juce::StringArray { "DETUNE", "DRIFT" }, false),
               { 180, row ("TUNE MODE"), 220, 24 });
        place (std::make_unique<Knob> (param ("tune"), "TUNE", true), { 180, row ("TUNE"), 70, 76 });
    }

    static constexpr int faderHeight = rowHeight - sectionTitle - 20;

    RotorAudioProcessor& processor;
    PresetManager& presets;
    std::vector<Section> sections;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> captions;
    std::vector<std::unique_ptr<juce::Component>> owned;
    juce::Array<SlotStrip*> slotStrips;
    juce::TextButton prev, next, presetName, save, fullRow, optionsButton;
    std::unique_ptr<OptionsPanel> options;
};

// ---------------------------------------------------------------------------------------------

RotorEditor::RotorEditor (RotorAudioProcessor& p) : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    panel = std::make_unique<Panel> (p);
    addAndMakeVisible (*panel);

    // Scalable: the panel is drawn at its design size and scaled to the window.
    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth * 3 / 4, designHeight * 3 / 4);
    startTimerHz (8);
}

RotorEditor::~RotorEditor()
{
    setLookAndFeel (nullptr);
}

void RotorEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    panel->setTransform (juce::AffineTransform::scale (scale));
}

void RotorEditor::timerCallback()
{
    panel->refresh();
}
