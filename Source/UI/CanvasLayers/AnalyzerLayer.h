#pragma once
#include "CanvasContext.h"

namespace graphite
{
/** Pre and post spectra, behind everything, deliberately recessive. */
struct AnalyzerLayer
{
    static void paint (juce::Graphics&, const CanvasContext&);
};
}
