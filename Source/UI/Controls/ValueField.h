#pragma once

#include "LookAndFeel.h"

namespace graphite
{

/**
    A caption and a tinted number: `Bands  [ 12 ]`.

    Drag it to change, double-click to type. The tint is the point - a field
    that holds a value the user can change is blue everywhere on the panel, and
    a label that only reports something is not. That is the whole affordance,
    and it costs no chrome.
*/
class ValueField final : public juce::Component
{
public:
    ValueField (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
                const juce::String& caption);
    ~ValueField() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void refreshText();

    juce::String caption;
    juce::Slider slider { juce::Slider::LinearBar, juce::Slider::NoTextBox };
    juce::Label value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::RangedAudioParameter* parameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueField)
};

} // namespace graphite
