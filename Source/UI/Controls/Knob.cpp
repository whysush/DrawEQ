#include "Knob.h"

namespace graphite
{

GraphiteLookAndFeel::GraphiteLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, Theme::Colour::of (Theme::Colour::plot));
    setColour (juce::Slider::rotarySliderOutlineColourId, Theme::Colour::of (Theme::Colour::recessed));
    setColour (juce::Label::textColourId, Theme::Colour::of (Theme::Colour::textMid));
    setColour (juce::TextButton::buttonColourId, Theme::Colour::of (Theme::Colour::raised));
    setColour (juce::TextButton::buttonOnColourId, Theme::Colour::of (Theme::Colour::raised));
    setColour (juce::TextEditor::backgroundColourId, Theme::Colour::of (Theme::Colour::recessed));
    setColour (juce::TextEditor::textColourId, Theme::Colour::of (Theme::Colour::textHi));
    setColour (juce::TextEditor::highlightColourId, Theme::Colour::of (Theme::Colour::focus));
    setColour (juce::CaretComponent::caretColourId, Theme::Colour::of (Theme::Colour::plot));
}

void GraphiteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float track = 2.5f;

    juce::Path back;
    back.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f,
                        startAngle, endAngle, true);
    g.setColour (Theme::Colour::of (Theme::Colour::recessed));
    g.strokePath (back, juce::PathStrokeType (track, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // Bipolar parameters fill outward from the centre, so "no change" reads as
    // an empty dial rather than a half-full one.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float originPos = bipolar ? float ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum()))
                                    : 0.0f;
    const float originAngle = startAngle + originPos * (endAngle - startAngle);

    juce::Path fill;
    fill.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f,
                        juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);
    g.setColour (Theme::Colour::of (Theme::Colour::plot).withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (fill, juce::PathStrokeType (track, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.startNewSubPath (centre.x, centre.y - radius * 0.45f);
    pointer.lineTo (centre.x, centre.y - radius + track * 1.6f);
    g.setColour (Theme::Colour::of (Theme::Colour::textHi));
    g.strokePath (pointer, juce::PathStrokeType (1.6f),
                  juce::AffineTransform::rotation (angle, centre.x, centre.y));

    if (s.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawEllipse (bounds.expanded (1.5f), Theme::Metrics::focusRing);
    }
}

void GraphiteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    g.setColour (Theme::Colour::of (on ? Theme::Colour::raised : Theme::Colour::panel)
                     .brighter (highlighted ? 0.12f : 0.0f)
                     .darker (down ? 0.15f : 0.0f));
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (Theme::Colour::of (on ? Theme::Colour::plot : Theme::Colour::hairline)
                     .withAlpha (on ? 0.8f : 1.0f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    if (b.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRoundedRectangle (bounds.expanded (1.0f), 3.0f, Theme::Metrics::focusRing);
    }
}

void GraphiteLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                          bool highlighted, bool)
{
    const auto colour = b.getToggleState() ? Theme::Colour::of (Theme::Colour::textHi)
                      : highlighted        ? Theme::Colour::of (Theme::Colour::textMid)
                                           : Theme::Colour::of (Theme::Colour::textLo);

    Theme::drawTrackedLabel (g, b.getButtonText(), b.getLocalBounds(), colour,
                             juce::Justification::centred);
}

juce::Font GraphiteLookAndFeel::getLabelFont (juce::Label&)
{
    return Theme::monoFont (Theme::Metrics::smallSize);
}

// ---------------------------------------------------------------------------

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
            const juce::String& label)
    : caption (label)
{
    parameter = state.getParameter (parameterID);

    slider.setWantsKeyboardFocus (true);
    slider.setDoubleClickReturnValue (true, parameter != nullptr
                                            ? double (parameter->convertFrom0to1 (
                                                  parameter->getDefaultValue()))
                                            : 0.0);
    slider.setVelocityBasedMode (false);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centred);
    value.setEditable (false, true, false);   // single click selects, double click edits
    value.setColour (juce::Label::textColourId, Theme::Colour::of (Theme::Colour::textHi));
    value.setColour (juce::Label::backgroundWhenEditingColourId,
                     Theme::Colour::of (Theme::Colour::recessed));
    addAndMakeVisible (value);

    value.onTextChange = [this]
    {
        if (parameter == nullptr)
            return;

        // Typed readouts (CONTEXT.md 9.7). Anything unparseable simply snaps
        // back to the current value on the next refresh.
        const auto text = value.getText().retainCharacters ("-0123456789.");

        if (text.isNotEmpty())
            slider.setValue (text.getDoubleValue(), juce::sendNotificationSync);

        refreshText();
    };

    slider.onValueChange = [this] { refreshText(); };

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterID, slider);

    refreshText();
}

Knob::~Knob() = default;

void Knob::refreshText()
{
    if (parameter == nullptr)
        return;

    // JUCE formats floats with two decimals, which turns "100 %" into
    // "100.00" and wastes a third of the width on zeros that never change.
    auto text = parameter->getCurrentValueAsText();

    if (text.containsChar ('.'))
    {
        while (text.endsWithChar ('0'))
            text = text.dropLastCharacters (1);

        if (text.endsWithChar ('.'))
            text = text.dropLastCharacters (1);
    }

    value.setText (text, juce::dontSendNotification);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (12);                     // caption, drawn in paint
    value.setBounds (area.removeFromBottom (14));
    slider.setBounds (area.reduced (2));
}

void Knob::paint (juce::Graphics& g)
{
    Theme::drawTrackedLabel (g, caption, getLocalBounds().removeFromTop (12),
                             Theme::Colour::of (Theme::Colour::textLo),
                             juce::Justification::centred);
}

} // namespace graphite
