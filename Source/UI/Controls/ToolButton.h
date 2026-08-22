#pragma once

#include "../Theme.h"
#include "../Tools/DrawTools.h"

namespace graphite
{

/** One tool in the bottom bar: a vector glyph in a bordered cell, lit when the
    tool is active. Focusable and space-activated like every other control. */
class ToolButton final : public juce::Button
{
public:
    explicit ToolButton (Tool toolToShow);

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    /** Draws the glyph alone, for anything that needs the icon without the
        cell around it. */
    static void drawIcon (juce::Graphics&, Tool, juce::Rectangle<float> area,
                          juce::Colour colour, float intensity);

private:
    Tool tool;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolButton)
};

} // namespace graphite
