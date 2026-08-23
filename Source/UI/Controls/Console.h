#pragma once

#include "LookAndFeel.h"

namespace draweq
{

class DrawEQProcessor;

/**
    The status lines under the sidebar.

    Every figure here is measured, not decorated: the band count and fit error
    come from the worker's last publish, the latency is what the host was told,
    and the load is the audio callback timing itself. A panel that reported a
    plausible-looking constant would be worse than no panel.
*/
class Console final : public juce::Component,
                      private juce::Timer
{
public:
    explicit Console (DrawEQProcessor&);
    ~Console() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    DrawEQProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Console)
};

} // namespace draweq
