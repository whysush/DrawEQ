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

void GraphiteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // A dark body, then the value as a thin arc outside it. The body is what
    // makes the control read as a physical knob; the arc is what makes it read
    // as a number, and the two jobs are better done by separate marks than by
    // one bevelled dial trying to do both.
    const float bodyRadius = radius * 0.72f;
    g.setColour (Theme::Colour::of (Theme::Colour::board));
    g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f)
                       .withCentre (centre));

    const float arcRadius = radius - 1.5f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         startAngle, endAngle, true);
    g.setColour (Theme::Colour::of (Theme::Colour::recessed));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Bipolar parameters fill outward from the centre, so "no change" reads as
    // an empty dial rather than a half-full one.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float originPos = bipolar
        ? float ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : 0.0f;
    const float originAngle = startAngle + originPos * (endAngle - startAngle);

    juce::Path fill;
    fill.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);
    g.setColour (Theme::Colour::of (s.isEnabled() ? Theme::Colour::plot : Theme::Colour::recessed));
    g.strokePath (fill, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.startNewSubPath (centre.x, centre.y - bodyRadius * 0.25f);
    pointer.lineTo (centre.x, centre.y - bodyRadius + 1.5f);
    g.setColour (Theme::Colour::of (Theme::Colour::textOnPlate));
    g.strokePath (pointer, juce::PathStrokeType (1.8f),
                  juce::AffineTransform::rotation (angle, centre.x, centre.y));

    if (s.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawEllipse (bounds, Theme::Metrics::focusRing);
    }
}

void GraphiteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    g.setColour (Theme::Colour::of (on ? Theme::Colour::accent : Theme::Colour::raised)
                     .brighter (highlighted ? 0.08f : 0.0f)
                     .darker (down ? 0.10f : 0.0f));
    g.fillRoundedRectangle (bounds, 2.0f);

    g.setColour (Theme::Colour::of (Theme::Colour::recessed).withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

    if (b.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, Theme::Metrics::focusRing);
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
    g.fillRoundedRectangle (bounds, 2.0f);
    g.setColour (Theme::Colour::of (box.hasKeyboardFocus (false) ? Theme::Colour::focus
                                                                 : Theme::Colour::recessed));
    g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

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
