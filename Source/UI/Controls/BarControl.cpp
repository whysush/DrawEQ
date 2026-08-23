#include "BarControl.h"

namespace graphite
{

namespace
{
    // Narrow on purpose: every pixel here is one the bar does not get, and a
    // bar with six cells in it is not a bar.
    constexpr int kValueWidth = 32;

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

BarControl::BarControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                        const juce::String& captionToShow, int captionWidthToUse)
    : caption (captionToShow), captionWidth (captionWidthToUse)
{
    parameter = state.getParameter (parameterID);

    // A LinearBar slider with nothing drawn is the least fussy way to get
    // JUCE's drag, wheel, arrow-key and double-click-to-reset behaviour under
    // something that draws itself.
    slider.setWantsKeyboardFocus (true);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    slider.setDoubleClickReturnValue (true, parameter != nullptr
                                            ? double (parameter->convertFrom0to1 (
                                                  parameter->getDefaultValue()))
                                            : 0.0);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centredRight);
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

BarControl::BarControl (const juce::String& captionToShow, juce::Range<double> range,
                        double interval, const juce::String& suffixToShow, int captionWidthToUse)
    : caption (captionToShow), suffix (suffixToShow), captionWidth (captionWidthToUse)
{
    slider.setWantsKeyboardFocus (true);
    slider.setRange (range.getStart(), range.getEnd(), interval);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    value.setJustificationType (juce::Justification::centredRight);
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

BarControl::~BarControl() = default;

void BarControl::setSkewForFrequency()
{
    // Frequency is heard logarithmically, so the bar has to travel that way too
    // or the bottom four octaves live in the first two cells.
    slider.setSkewFactorFromMidPoint (std::sqrt (slider.getMinimum() * slider.getMaximum()));
}

void BarControl::setEnabledLook (bool shouldBeEnabled)
{
    slider.setEnabled (shouldBeEnabled);
    value.setEnabled (shouldBeEnabled);
    showValue = shouldBeEnabled;
    refreshText();
    repaint();
}

void BarControl::setValue (double v, juce::NotificationType n)
{
    slider.setValue (v, n);
    refreshText();
}

void BarControl::refreshText()
{
    if (! showValue)
    {
        // A number under a dead bar is a number about nothing.
        value.setText (juce::String::fromUTF8 ("\xe2\x80\x93"), juce::dontSendNotification);
        repaint();
        return;
    }

    if (parameter != nullptr)
    {
        value.setText (trimZeros (parameter->getCurrentValueAsText()), juce::dontSendNotification);
        repaint();
        return;
    }

    const double v = slider.getValue();

    value.setText (suffix == "Hz" && v >= 1000.0
                       ? trimZeros (juce::String (v / 1000.0, 2)) + "k"
                       : trimZeros (juce::String (v, v < 10.0 ? 2 : 0)),
                   juce::dontSendNotification);
    repaint();
}

void BarControl::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft (captionWidth);
    value.setBounds (area.removeFromRight (kValueWidth));
    slider.setBounds (area);
}

void BarControl::paint (juce::Graphics& g)
{
    Theme::drawTrackedLabel (g, caption, getLocalBounds().withWidth (captionWidth),
                             Theme::Colour::of (showValue ? Theme::Colour::textMid
                                                          : Theme::Colour::textLo),
                             juce::Justification::centredLeft);

    const auto track = slider.getBounds().reduced (0, 5);

    if (track.getWidth() <= 0)
        return;

    const int pitch = Theme::Metrics::barCellWidth + Theme::Metrics::barCellGap;
    const int cells = juce::jmax (1, track.getWidth() / pitch);

    const double range = slider.getMaximum() - slider.getMinimum();
    const double norm  = range > 0.0 ? (slider.getValue() - slider.getMinimum()) / range : 0.0;

    // Bipolar values fill outward from the middle, so "no change" reads as an
    // empty bar rather than a half-full one.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const double originNorm = bipolar
        ? (0.0 - slider.getMinimum()) / range : 0.0;

    const int valueCell  = juce::jlimit (0, cells, int (std::lround (norm * double (cells))));
    const int originCell = juce::jlimit (0, cells, int (std::lround (originNorm * double (cells))));

    const int lo = juce::jmin (valueCell, originCell);
    const int hi = juce::jmax (valueCell, originCell);

    const auto lit   = Theme::Colour::of (showValue ? Theme::Colour::plot : Theme::Colour::textLo);
    const auto unlit = Theme::Colour::of (Theme::Colour::recessed);

    for (int i = 0; i < cells; ++i)
    {
        const bool on = i >= lo && i < hi;
        g.setColour (on ? lit : unlit);
        g.fillRect (track.getX() + i * pitch, track.getY(),
                    Theme::Metrics::barCellWidth, track.getHeight());
    }

    // A single brighter cell marks the value itself, so a bipolar bar still
    // says which side of centre it is on when it is nearly empty.
    if (showValue && cells > 0)
    {
        const int marker = juce::jlimit (0, cells - 1, valueCell - (valueCell > originCell ? 1 : 0));
        g.setColour (Theme::Colour::of (Theme::Colour::titleText));
        g.fillRect (track.getX() + marker * pitch, track.getY(),
                    Theme::Metrics::barCellWidth, track.getHeight());
    }

    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (slider.getBounds(), int (Theme::Metrics::focusRing));
    }
}

} // namespace graphite
