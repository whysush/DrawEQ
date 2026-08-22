#pragma once

#include "LookAndFeel.h"

namespace graphite
{

/**
    One parameter row: `MORPH  [========|.......]  0`.

    Label on the left, the character bar in the middle, the number on the right.
    All the interaction is JUCE's - drag, wheel, arrow keys, double-click to
    reset, Ctrl to fine-adjust - and the number is editable by typing, which is
    the quality floor CONTEXT.md 9.7 asks to be built in rather than retrofitted.
*/
class BarSlider final : public juce::Component
{
public:
    BarSlider (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
               const juce::String& label);
    ~BarSlider() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void refreshText();

    juce::String caption;
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label  value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::RangedAudioParameter* parameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarSlider)
};

} // namespace graphite
