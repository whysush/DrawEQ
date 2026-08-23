#include "LookAndFeel.h"

namespace graphite
{

GraphiteLookAndFeel::GraphiteLookAndFeel()
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

void GraphiteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    g.setColour (Theme::Colour::of (on ? Theme::Colour::accent : Theme::Colour::raised)
                     .brighter (highlighted ? 0.08f : 0.0f)
                     .darker (down ? 0.10f : 0.0f));
    g.fillRect (bounds);

    g.setColour (Theme::Colour::of (Theme::Colour::recessed).withAlpha (0.7f));
    g.drawRect (bounds, 1.0f);

    if (b.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (bounds, Theme::Metrics::focusRing);
    }
}

void GraphiteLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                          bool highlighted, bool)
{
    juce::ignoreUnused (highlighted);

    Theme::drawTrackedLabel (g, b.getButtonText(), b.getLocalBounds(),
                             Theme::Colour::of (Theme::Colour::textHi),
                             juce::Justification::centred);
}

void GraphiteLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

    g.setColour (Theme::Colour::of (Theme::Colour::raised));
    g.fillRect (bounds);
    g.setColour (Theme::Colour::of (box.hasKeyboardFocus (false) ? Theme::Colour::focus
                                                                 : Theme::Colour::recessed));
    g.drawRect (bounds, 1.0f);

    // Drawn rather than typed: a triangle glyph is exactly the sort of
    // character a bundled face may not carry, and a missing one renders as tofu.
    const auto cell = bounds.removeFromRight (16.0f);
    const auto c = cell.getCentre();

    juce::Path caret;
    caret.startNewSubPath (c.x - 3.5f, c.y - 2.0f);
    caret.lineTo (c.x + 3.5f, c.y - 2.0f);
    caret.lineTo (c.x, c.y + 2.5f);
    caret.closeSubPath();

    g.setColour (Theme::Colour::of (Theme::Colour::textHi));
    g.fillPath (caret);
}

void GraphiteLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Font GraphiteLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::labelFont (Theme::Metrics::labelSize);
}

juce::Font GraphiteLookAndFeel::getPopupMenuFont()
{
    return Theme::labelFont (Theme::Metrics::bodySize);
}

juce::Font GraphiteLookAndFeel::getLabelFont (juce::Label&)
{
    return Theme::monoFont (Theme::Metrics::smallSize);
}

} // namespace graphite
