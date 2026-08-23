#include "ValueField.h"

namespace graphite
{

namespace
{
    constexpr int kCaptionHeight = 14;
}

ValueField::ValueField (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                        const juce::String& captionToShow)
    : caption (captionToShow)
{
    parameter = state.getParameter (parameterID);

    // A LinearBar slider with nothing drawn is the least fussy way to get
    // JUCE's drag, wheel, arrow-key and double-click-to-reset behaviour on
    // something that does not look like a slider.
    slider.setWantsKeyboardFocus (true);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setDoubleClickReturnValue (true, parameter != nullptr
                                            ? double (parameter->convertFrom0to1 (
                                                  parameter->getDefaultValue()))
                                            : 0.0);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centred);
    value.setEditable (false, true, false);
    value.setInterceptsMouseClicks (false, false);   // the slider under it takes the drag
    value.setColour (juce::Label::textColourId, Theme::Colour::of (Theme::Colour::fieldText));
    value.setColour (juce::Label::backgroundWhenEditingColourId,
                     Theme::Colour::of (Theme::Colour::field));
    addAndMakeVisible (value);

    slider.onValueChange = [this] { refreshText(); };

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterID, slider);

    refreshText();
}

ValueField::~ValueField() = default;

void ValueField::refreshText()
{
    if (parameter == nullptr)
        return;

    auto text = parameter->getCurrentValueAsText();

    // JUCE formats floats with two decimals, which spends most of a narrow
    // field on zeros that never change.
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

void ValueField::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (kCaptionHeight);
    slider.setBounds (area);
    value.setBounds (area);
}

void ValueField::paint (juce::Graphics& g)
{
    Theme::drawTrackedLabel (g, caption, getLocalBounds().withHeight (kCaptionHeight),
                             Theme::Colour::of (Theme::Colour::textMid),
                             juce::Justification::centredLeft);

    const auto box = getLocalBounds().withTrimmedTop (kCaptionHeight).toFloat().reduced (0.5f);

    g.setColour (Theme::Colour::of (Theme::Colour::field));
    g.fillRoundedRectangle (box, 2.0f);

    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRoundedRectangle (box, 2.0f, Theme::Metrics::focusRing);
    }
}

} // namespace graphite
