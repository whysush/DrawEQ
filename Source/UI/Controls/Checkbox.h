#pragma once

#include "LookAndFeel.h"

namespace draweq
{

/** `[x] INVERT` - a square, a mark, a tracked label. Tab-reachable and
    space-activated, like everything else. */
class Checkbox final : public juce::Component
{
public:
    Checkbox (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
              const juce::String& label);
    ~Checkbox() override;

    void resized() override;

private:
    class Box;
    std::unique_ptr<Box> box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Checkbox)
};

} // namespace draweq
