#pragma once

#include "LookAndFeel.h"

namespace draweq
{

class DrawEQProcessor;

/**
    The eight preset slots, across the top of the window.

    Click selects the slot the canvas edits (which is slot A of the morph, so
    "what you drew" and "the morph source" are the same thing and morph is
    continuous at zero). Shift-click stores, Alt-click clears, right-click sets
    the morph target.
*/
class SlotStrip final : public juce::Component,
                        private juce::Timer
{
public:
    explicit SlotStrip (DrawEQProcessor&);
    ~SlotStrip() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    int slotAt (juce::Point<float>) const;
    juce::Rectangle<float> boundsForSlot (int) const;

    DrawEQProcessor& processor;
    int hovered = -1;
    int focused = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotStrip)
};

} // namespace draweq
