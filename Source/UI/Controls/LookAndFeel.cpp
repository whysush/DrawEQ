#include "LookAndFeel.h"

namespace draweq
{

DrawEQLookAndFeel::DrawEQLookAndFeel()
{
    setColour (juce::Label::textColourId,          Theme::Colour::of (Theme::Colour::textHi));
    setColour (juce::TextButton::buttonColourId,   Theme::Colour::of (Theme::Colour::raised));
    setColour (juce::TextButton::buttonOnColourId, Theme::Colour::of (Theme::Colour::accent));
    setColour (juce::TextEditor::backgroundColourId, Theme::Colour::of (Theme::Colour::field));
    setColour (juce::TextEditor::textColourId,     Theme::Colour::of (Theme::Colour::fieldText));
    setColour (juce::TextEditor::highlightColourId, Theme::Colour::of (Theme::Colour::focus));
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::Colour::of (Theme::Colour::focus));
    setColour (juce::CaretComponent::caretColourId, Theme::Colour::of (Theme::Colour::fieldText));
    setColour (juce::PopupMenu::backgroundColourId, Theme::Colour::of (Theme::Colour::raised));
    setColour (juce::PopupMenu::textColourId,      Theme::Colour::of (Theme::Colour::textHi));
    setColour (juce::PopupMenu::highlightedBackgroundColourId,
               Theme::Colour::of (Theme::Colour::accent));
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::Colour::of (Theme::Colour::textHi));
}

void DrawEQLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = b.getLocalBounds();
    const bool on = b.getToggleState();

    g.setColour (Theme::Colour::of (on ? Theme::Colour::accent : Theme::Colour::raised)
                     .brighter (highlighted ? 0.08f : 0.0f)
                     .darker (down ? 0.10f : 0.0f));
    g.fillRect (bounds);

    // Pressed reads as recessed rather than merely darker, which is the one
    // piece of feedback a flat panel otherwise has no way to give.
    Theme::drawPixelBevel (g, bounds, ! down);

    if (b.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (bounds, int (Theme::Metrics::focusRing));
    }
}

void DrawEQLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                          bool highlighted, bool)
{
    juce::ignoreUnused (highlighted);

    Theme::drawTrackedLabel (g, b.getButtonText(), b.getLocalBounds(),
                             Theme::Colour::of (Theme::Colour::textHi),
                             juce::Justification::centred);
}

void DrawEQLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height);

    g.setColour (Theme::Colour::of (Theme::Colour::raised));
    g.fillRect (bounds);
    Theme::drawPixelBevel (g, bounds, true);

    if (box.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (bounds, int (Theme::Metrics::focusRing));
    }

    // Stepped rather than drawn as a triangle path: a stack of shortening rows
    // is a caret that lands on whole pixels, and a glyph would depend on a face
    // that may not carry it.
    const auto cell = bounds.removeFromRight (18);
    const int cx = cell.getCentreX(), cy = cell.getCentreY();

    g.setColour (Theme::Colour::of (Theme::Colour::textHi));

    for (int row = 0; row < 4; ++row)
        g.fillRect (cx - 3 + row, cy - 2 + row, 7 - row * 2, 1);
}

void DrawEQLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Font DrawEQLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::labelFont (Theme::Metrics::labelSize);
}

juce::Font DrawEQLookAndFeel::getPopupMenuFont()
{
    return Theme::labelFont (Theme::Metrics::bodySize);
}

juce::Font DrawEQLookAndFeel::getLabelFont (juce::Label&)
{
    return Theme::monoFont (Theme::Metrics::smallSize);
}

} // namespace draweq
