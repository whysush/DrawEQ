#include "Knob.h"

namespace graphite
{

namespace
{
    juce::String trimZeros (juce::String text)
    {
        if (! text.containsChar ('.'))
            return text;

        while (text.endsWithChar ('0'))
            text = text.dropLastCharacters (1);

        if (text.endsWithChar ('.'))
            text = text.dropLastCharacters (1);

        return text;
    }
}

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
            const juce::String& captionToShow)
    : caption (captionToShow)
{
    parameter = state.getParameter (parameterID);

    slider.setWantsKeyboardFocus (true);
    slider.setDoubleClickReturnValue (true, parameter != nullptr
                                            ? double (parameter->convertFrom0to1 (
                                                  parameter->getDefaultValue()))
                                            : 0.0);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centred);
    value.setEditable (false, true, false);
    value.setColour (juce::Label::textColourId, Theme::Colour::of (Theme::Colour::textHi));
    value.setColour (juce::Label::backgroundWhenEditingColourId,
                     Theme::Colour::of (Theme::Colour::field));
    addAndMakeVisible (value);

    value.onTextChange = [this]
    {
        const auto text = value.getText().retainCharacters ("-0123456789.");

        if (text.isNotEmpty())
            slider.setValue (text.getDoubleValue(), juce::sendNotificationSync);

        refreshText();
    };

    slider.onValueChange = [this]
    {
        refreshText();

        if (onValueChanged != nullptr)
            onValueChanged (slider.getValue());
    };

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterID, slider);

    refreshText();
}

Knob::Knob (const juce::String& captionToShow, juce::Range<double> range, double interval,
            const juce::String& suffixToShow)
    : caption (captionToShow), suffix (suffixToShow)
{
    slider.setWantsKeyboardFocus (true);
    slider.setRange (range.getStart(), range.getEnd(), interval);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centred);
    value.setEditable (false, true, false);
    value.setColour (juce::Label::textColourId, Theme::Colour::of (Theme::Colour::textHi));
    value.setColour (juce::Label::backgroundWhenEditingColourId,
                     Theme::Colour::of (Theme::Colour::field));
    addAndMakeVisible (value);

    value.onTextChange = [this]
    {
        const auto text = value.getText().retainCharacters ("-0123456789.");

        if (text.isNotEmpty())
            slider.setValue (text.getDoubleValue(), juce::sendNotificationSync);

        refreshText();
    };

    slider.onValueChange = [this]
    {
        refreshText();

        if (onValueChanged != nullptr)
            onValueChanged (slider.getValue());
    };

    refreshText();
}

Knob::~Knob() = default;

void Knob::setSkewForFrequency()
{
    // Frequency is perceived logarithmically, so the dial has to travel that
    // way too or the bottom four octaves live in the first millimetre.
    slider.setSkewFactorFromMidPoint (std::sqrt (slider.getMinimum() * slider.getMaximum()));
}

void Knob::setEnabledLook (bool shouldBeEnabled)
{
    slider.setEnabled (shouldBeEnabled);
    value.setEnabled (shouldBeEnabled);
    setAlpha (shouldBeEnabled ? 1.0f : 0.45f);

    // A number under a dead dial is a number about nothing, and reading "20 Hz"
    // when no band is selected is worse than reading nothing at all.
    showValue = shouldBeEnabled;
    refreshText();
}

void Knob::setValue (double v, juce::NotificationType n)
{
    slider.setValue (v, n);
    refreshText();
}

void Knob::refreshText()
{
    if (! showValue)
    {
        value.setText (juce::String::fromUTF8 ("\xe2\x80\x93"), juce::dontSendNotification);
        return;
    }

    if (parameter != nullptr)
    {
        value.setText (trimZeros (parameter->getCurrentValueAsText()), juce::dontSendNotification);
        return;
    }

    const double v = slider.getValue();

    // Frequencies read in kHz once they get long enough to crowd the column.
    juce::String text = suffix == "Hz" && v >= 1000.0
                      ? juce::String (v / 1000.0, 2) + " kHz"
                      : trimZeros (juce::String (v, v < 10.0 ? 2 : 1))
                            + (suffix.isEmpty() ? juce::String() : " " + suffix);

    value.setText (text, juce::dontSendNotification);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (13);                       // caption, painted
    value.setBounds (area.removeFromBottom (14));
    slider.setBounds (area.reduced (2));
}

void Knob::paint (juce::Graphics& g)
{
    Theme::drawTrackedLabel (g, caption, getLocalBounds().removeFromTop (13),
                             Theme::Colour::of (Theme::Colour::textMid),
                             juce::Justification::centred);
}

} // namespace graphite
