#pragma once

#include "../Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace graphite
{

/**
    Shared drawing for every control, so a focus ring, a hover state and a
    hairline mean the same thing everywhere.

    The linear slider is drawn as a run of monospaced characters -
    `[========|.......]` - rather than as a filled track. It is not decoration:
    the cells quantise the value visually, so two parameters at the same setting
    line up exactly, and a glance down the column reads like a column of
    numbers rather than a row of unrelated bars.
*/
class GraphiteLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GraphiteLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
};

} // namespace graphite
