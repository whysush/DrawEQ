#pragma once

#include "../Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace graphite
{

/**
    Shared drawing for every control, so that a focus ring, a hover state and a
    hairline mean the same thing everywhere.
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

    juce::Font getLabelFont (juce::Label&) override;
};

/**
    Label in Grotesk, value in Mono, never the other way round (CONTEXT.md 9.3).

    Reachable by Tab, adjustable by arrow keys, reset by double-clicking the
    dial, and editable by double-clicking the number - the quality floor from
    CONTEXT.md 9.7, built in rather than retrofitted.
*/
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
          const juce::String& label);
    ~Knob() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void refreshText();

    juce::String caption;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag,
                          juce::Slider::NoTextBox };
    juce::Label value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::RangedAudioParameter* parameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

} // namespace graphite
