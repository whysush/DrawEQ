#include "LookAndFeel.h"

namespace graphite
{

GraphiteLookAndFeel::GraphiteLookAndFeel()
{
    setColour (juce::Label::textColourId,          Theme::Colour::of (Theme::Colour::textHi));
    setColour (juce::TextButton::buttonColourId,   Theme::Colour::of (Theme::Colour::recessed));
    setColour (juce::TextButton::buttonOnColourId, Theme::Colour::of (Theme::Colour::raised));
    setColour (juce::TextEditor::backgroundColourId, Theme::Colour::of (Theme::Colour::recessed));
    setColour (juce::TextEditor::textColourId,     Theme::Colour::of (Theme::Colour::textHi));
    setColour (juce::TextEditor::highlightColourId, Theme::Colour::of (Theme::Colour::accentDim));
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::Colour::of (Theme::Colour::accent));
    setColour (juce::CaretComponent::caretColourId, Theme::Colour::of (Theme::Colour::accent));
    setColour (juce::PopupMenu::backgroundColourId, Theme::Colour::of (Theme::Colour::panel));
    setColour (juce::PopupMenu::textColourId,      Theme::Colour::of (Theme::Colour::textMid));
    setColour (juce::PopupMenu::highlightedBackgroundColourId,
               Theme::Colour::of (Theme::Colour::raised));
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::Colour::of (Theme::Colour::accent));
}

void GraphiteLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto font = Theme::monoFont (Theme::Metrics::labelSize);
    g.setFont (font);

    // Monospace: every glyph has the same advance, so one measurement places
    // every cell exactly.
    const float charW = juce::jmax (1.0f, juce::GlyphArrangement::getStringWidth (font, "="));
    const int   cells = juce::jlimit (4, 48, int ((float (width) - charW * 2.0f) / charW) - 1);

    const double range = slider.getMaximum() - slider.getMinimum();
    const double norm  = range > 0.0 ? (slider.getValue() - slider.getMinimum()) / range : 0.0;
    const int    filled = juce::jlimit (0, cells, int (std::lround (norm * double (cells))));

    juce::ignoreUnused (sliderPos, minSliderPos, maxSliderPos);

    const float baseline = float (y) + float (height) * 0.5f + font.getHeight() * 0.34f;
    float penX = float (x);

    auto put = [&] (const juce::String& text, juce::Colour colour)
    {
        g.setColour (colour);

        for (int i = 0; i < text.length(); ++i)
        {
            g.drawSingleLineText (text.substring (i, i + 1), juce::roundToInt (penX),
                                  juce::roundToInt (baseline));
            penX += charW;
        }
    };

    const auto bracket = Theme::Colour::of (Theme::Colour::textLo);
    const auto lit     = Theme::Colour::of (slider.isEnabled() ? Theme::Colour::accent
                                                              : Theme::Colour::accentDim);
    const auto unlit   = Theme::Colour::of (Theme::Colour::graticuleM);

    put ("[", bracket);
    put (juce::String::repeatedString ("=", filled), lit);
    put ("|", Theme::Colour::of (Theme::Colour::textHi));
    put (juce::String::repeatedString (".", cells - filled), unlit);
    put ("]", bracket);

    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (juce::Rectangle<int> (x, y, width, height).toFloat().expanded (1.0f),
                    Theme::Metrics::focusRing);
    }
}

void GraphiteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    g.setColour (Theme::Colour::of (on ? Theme::Colour::raised : Theme::Colour::recessed)
                     .brighter (highlighted ? 0.10f : 0.0f)
                     .darker (down ? 0.12f : 0.0f));
    g.fillRect (bounds);

    g.setColour (Theme::Colour::of (on ? Theme::Colour::accent : Theme::Colour::hairline));
    g.drawRect (bounds, on ? 1.4f : 1.0f);

    if (b.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (bounds.expanded (1.0f), Theme::Metrics::focusRing);
    }
}

void GraphiteLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                          bool highlighted, bool)
{
    const auto colour = b.getToggleState() ? Theme::Colour::of (Theme::Colour::accent)
                      : highlighted        ? Theme::Colour::of (Theme::Colour::textHi)
                                           : Theme::Colour::of (Theme::Colour::textMid);

    Theme::drawTrackedLabel (g, b.getButtonText(), b.getLocalBounds(), colour,
                             juce::Justification::centred);
}

void GraphiteLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

    g.setColour (Theme::Colour::of (Theme::Colour::recessed));
    g.fillRect (bounds);
    g.setColour (Theme::Colour::of (box.hasKeyboardFocus (false) ? Theme::Colour::accent
                                                                 : Theme::Colour::hairline));
    g.drawRect (bounds, 1.0f);

    // Drawn rather than typed: a triangle glyph is exactly the sort of
    // character a bundled face may not carry, and a missing one renders as
    // tofu.
    const auto arrowCell = bounds.removeFromRight (20.0f);
    const float w = 7.0f, h = 4.0f;
    const auto c = arrowCell.getCentre();

    juce::Path caret;
    caret.startNewSubPath (c.x - w * 0.5f, c.y - h * 0.5f);
    caret.lineTo (c.x + w * 0.5f, c.y - h * 0.5f);
    caret.lineTo (c.x, c.y + h * 0.5f);
    caret.closeSubPath();

    g.setColour (Theme::Colour::of (Theme::Colour::textMid));
    g.fillPath (caret);
}

void GraphiteLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Font GraphiteLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::monoFont (Theme::Metrics::labelSize);
}

juce::Font GraphiteLookAndFeel::getPopupMenuFont()
{
    return Theme::monoFont (Theme::Metrics::smallSize);
}

juce::Font GraphiteLookAndFeel::getLabelFont (juce::Label&)
{
    return Theme::monoFont (Theme::Metrics::smallSize);
}

} // namespace graphite
