#pragma once
#include "CanvasContext.h"

namespace draweq
{

/**
    The mascot, stuck in a corner of the plate.

    Drawn after the graticule but *before* the curve, which is the one liberty
    taken with "sticker": he is fully opaque and framed like something pressed
    onto the panel, but the response line still passes over him. A picture that
    could hide the measurement would be a picture that costs something, and this
    one is here to be enjoyed rather than consulted.
*/
struct StickerLayer
{
    static void paint (juce::Graphics&, const CanvasContext&, const juce::Image&);
};

}
