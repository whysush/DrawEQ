#include "BarSlider.h"

namespace graphite
{

namespace
{
    constexpr int kLabelWidth = 78;
    constexpr int kValueWidth = 52;
}

BarSlider::BarSlider (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                      const juce::String& label)
    : caption (label)
{
    parameter = state.getParameter (parameterID);

    slider.setWantsKeyboardFocus (true);
    slider.setDoubleClickReturnValue (true, parameter != nullptr
                                            ? double (parameter->convertFrom0to1 (
                                                  parameter->getDefaultValue()))
                                            : 0.0);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centredRight);
    value.setEditable (false, true, false);
    value.setColour (juce::Label::textColourId, Theme::Colour::of (Theme::Colour::textHi));
    value.setColour (juce::Label::backgroundWhenEditingColourId,
                     Theme::Colour::of (Theme::Colour::recessed));
    addAndMakeVisible (value);

    value.onTextChange = [this]
    {
        if (parameter == nullptr)
            return;

        // Anything unparseable simply snaps back on the next refresh, which is
        // friendlier than an error and impossible to get wrong.
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

BarSlider::~BarSlider() = default;

void BarSlider::refreshText()
{
    if (parameter == nullptr)
        return;

    // JUCE formats floats with two decimals, which spends a third of the
    // column on zeros that never change.
    auto text = parameter->getCurrentValueAsText();

    if (text.containsChar ('.'))
    {
        while (text.endsWithChar ('0'))
            text = text.dropLastCharacters (1);

        if (text.endsWithChar ('.'))
            text = text.dropLastCharacters (1);
    }

    value.setText (text, juce::dontSendNotification);
    repaint();
}

void BarSlider::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft (kLabelWidth);
    value.setBounds (area.removeFromRight (kValueWidth));
    slider.setBounds (area.reduced (4, 0));
}

void BarSlider::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (Theme::Colour::of (Theme::Colour::panel));
    g.fillRect (bounds);
    g.setColour (Theme::Colour::of (Theme::Colour::hairline));
    g.drawRect (bounds, 1.0f);

    Theme::drawTrackedLabel (g, caption, getLocalBounds().withWidth (kLabelWidth).reduced (10, 0),
                             Theme::Colour::of (Theme::Colour::textMid),
                             juce::Justification::centredLeft);
}

} // namespace graphite
