#pragma once
#include "CanvasContext.h"

namespace graphite
{
/** Draggable band handles, Analog mode only. */
struct BandTokenLayer
{
    static void paint (juce::Graphics&, const CanvasContext&);

    /** Index of the token under a point, or -1. Used for hit testing by the
        Node tool. */
    static int hitTest (const CanvasContext&, juce::Point<float>);
};
}
