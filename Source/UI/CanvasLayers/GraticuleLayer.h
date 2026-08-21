#pragma once
#include "CanvasContext.h"

namespace graphite
{
/** Hairline frequency and level grid, with labels placed to stay out of the
    curve's way. */
struct GraticuleLayer
{
    static void paint (juce::Graphics&, const CanvasContext&);
};
}
