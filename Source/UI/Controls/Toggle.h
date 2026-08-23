#pragma once

#include "LookAndFeel.h"

namespace draweq
{

/** A latching text button bound to a bool or choice parameter. Tab-reachable,
    space-activated, with the same focus ring as everything else. */
class Toggle final : public juce::Component
{
public:
    /** Bool parameter. */
    Toggle (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
            const juce::String& text);

    /** One value of a choice parameter, for a segmented row. */
    Toggle (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
            const juce::String& text, int choiceIndex);

    ~Toggle() override;

    void resized() override;

private:
    void syncFromParameter();

    juce::TextButton button;
    juce::AudioProcessorValueTreeState& state;
    juce::String paramID;
    int choice = -1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    std::unique_ptr<juce::ParameterAttachment> choiceAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Toggle)
};

} // namespace draweq
