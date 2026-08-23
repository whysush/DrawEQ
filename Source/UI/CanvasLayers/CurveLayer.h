#pragma once
#include "CanvasContext.h"

namespace draweq
{
/** The residual ribbon, the ghost stroke and the plot line - the three elements
    the whole interface is built around. */
struct CurveLayer
{
    static void paint (juce::Graphics&, const CanvasContext&);
};
}
