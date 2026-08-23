#include "Fader.h"

namespace graphite
{

namespace
{
    // Narrow on purpose: every pixel here is one the slot does not get.
    constexpr int kValueWidth = 40;

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

Fader::Fader (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
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

Fader::Fader (const juce::String& captionToShow, juce::Range<double> range,
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

Fader::~Fader() = default;

void Fader::setSkewForFrequency()
{
    // Frequency is heard logarithmically, so the bar has to travel that way too
    // or the bottom four octaves live in the first two cells.
    slider.setSkewFactorFromMidPoint (std::sqrt (slider.getMinimum() * slider.getMaximum()));
}

void Fader::setEnabledLook (bool shouldBeEnabled)
{
    slider.setEnabled (shouldBeEnabled);
    value.setEnabled (shouldBeEnabled);
    showValue = shouldBeEnabled;
    refreshText();
    repaint();
}

void Fader::setValue (double v, juce::NotificationType n)
{
    slider.setValue (v, n);
    refreshText();
}

void Fader::refreshText()
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

void Fader::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft (captionWidth);
    value.setBounds (area.removeFromRight (kValueWidth));
    slider.setBounds (area);
}

void Fader::paint (juce::Graphics& g)
{
    Theme::drawTrackedLabel (g, caption, getLocalBounds().withWidth (captionWidth),
                             Theme::Colour::of (showValue ? Theme::Colour::textMid
                                                          : Theme::Colour::textLo),
                             juce::Justification::centredLeft);

    const auto area = slider.getBounds();

    if (area.getWidth() <= Theme::Metrics::faderCapWidth)
        return;

    const int capW = Theme::Metrics::faderCapWidth;
    const int capH = juce::jmin (Theme::Metrics::faderCapHeight, area.getHeight());

    // The cap's centre can only reach half a cap-width from each end, so the
    // slot is inset to match. Otherwise the travel and the drawing disagree and
    // the cap appears to stop short of the ends.
    const int travelLeft  = area.getX() + capW / 2;
    const int travelRight = area.getRight() - capW / 2;
    const int travel = juce::jmax (1, travelRight - travelLeft);

    const auto slot = juce::Rectangle<int> (travelLeft - 2, area.getCentreY()
                                                - Theme::Metrics::faderSlotHeight / 2,
                                            travel + 4, Theme::Metrics::faderSlotHeight);

    g.setColour (Theme::Colour::of (Theme::Colour::recessed));
    g.fillRect (slot);
    Theme::drawPixelBevel (g, slot, false);

    const double range = slider.getMaximum() - slider.getMinimum();
    const double norm = range > 0.0 ? (slider.getValue() - slider.getMinimum()) / range : 0.0;
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;

    // Ticks along the slot, and a taller one at centre for a bipolar control -
    // the detent a console fader has moulded into it.
    g.setColour (Theme::Colour::of (Theme::Colour::hairline));

    for (int i = 0; i <= 8; ++i)
    {
        const int x = travelLeft + (travel * i) / 8;
        const bool centre = bipolar && i == 4;
        const int h = centre ? 5 : 3;
        g.fillRect (x, slot.getBottom() + 1, 1, h);
    }

    // The travelled part of the slot lights up, so the fader still says how
    // much as well as where.
    const int capX = travelLeft + int (std::lround (norm * double (travel)));
    const int originX = travelLeft + int (std::lround ((bipolar ? (0.0 - slider.getMinimum()) / range
                                                                : 0.0) * double (travel)));

    if (showValue)
    {
        const auto lit = juce::Rectangle<int>::leftTopRightBottom (
            juce::jmin (capX, originX), slot.getY() + 2,
            juce::jmax (capX, originX), slot.getBottom() - 2);

        g.setColour (Theme::Colour::of (Theme::Colour::plot).withAlpha (0.85f));
        g.fillRect (lit);
    }

    const auto cap = juce::Rectangle<int> (capW, capH)
                         .withCentre ({ capX, area.getCentreY() });

    g.setColour (Theme::Colour::of (showValue ? Theme::Colour::raised : Theme::Colour::panel));
    g.fillRect (cap);
    Theme::drawPixelBevel (g, cap, true);

    // The grip line down the middle of the cap: the one mark that makes a
    // rectangle read as a fader rather than a block.
    g.setColour (Theme::Colour::of (showValue ? Theme::Colour::titleText
                                              : Theme::Colour::textLo));
    g.fillRect (cap.getCentreX(), cap.getY() + 3, 1, cap.getHeight() - 6);

    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (area, int (Theme::Metrics::focusRing));
    }
}

} // namespace graphite
