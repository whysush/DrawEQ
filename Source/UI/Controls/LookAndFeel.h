#pragma once

#include "../Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace graphite
{

/**
    Shared drawing for every control, so a focus ring, a hover state and an
    "on" state mean the same thing everywhere.

    Two rules carry the whole look. Anything the user can grab or has switched
    on is amber; anything reporting what the filter is doing is blue. Nothing is
    bevelled, and the only depth in the panel comes from the plot plate being
    darker than the face it sits in.
*/
class GraphiteLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GraphiteLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

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
