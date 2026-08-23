#pragma once
#include "CanvasContext.h"

namespace draweq
{
/** Crosshair and readout, plus the permanent fit-quality line. */
struct CursorLayer
{
    static void paint (juce::Graphics&, const CanvasContext&, juce::Point<float> mouse, bool visible,
                       float brushOctaves, bool showBrush);

    static void paintErrorReadout (juce::Graphics&, const CanvasContext&);
};
}
