#pragma once

#include "../../Core/CurveModel.h"
#include "../../DSP/BandResponse.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace draweq
{

enum class Tool
{
    pencil = 0,
    line,
    smooth,
    erase,
    node
};

const char* toolName (Tool);
const char* toolGlyph (Tool);
juce::juce_wchar toolKey (Tool);

/** Maps a tool onto the brush the model understands. */
CurveModel::Brush brushFor (Tool);

/**
    Re-derives the curve from a band stack.

    This is the return leg of the Node tool, and it is what makes the tool
    bidirectional: drawing produces bands, dragging a band rewrites the curve,
    drawing again re-fits. No mode switch, no conversion step (CONTEXT.md 9.6).
*/
void curveFromBands (const Band* bands, int numBands, float trimDb, double sampleRate,
                     CurveArray& out);

} // namespace draweq
