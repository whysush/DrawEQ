#pragma once

#include "CurveSnapshot.h"

namespace graphite::shapes
{

/**
    Starting shapes: the handful of curves people actually reach for.

    Generated rather than stored. A shape built from bells and shelves is one
    the Analog fitter can reproduce almost exactly, which a captured 1024-point
    snapshot would not be, and generating them means they resample themselves to
    any future grid size for free.
*/
enum class Shape
{
    flat = 0,
    smiley,
    warmTilt,
    brightTilt,
    deMud,
    presence,
    air,
    rumbleCut,
    telephone,
    vocal,
    lowPass,
    count
};

const char* name (Shape);

/** Writes the shape onto the log grid. `sampleRate` only matters for the
    band-derived shapes, which are evaluated through the same closed form the
    filter uses. */
void build (Shape, double sampleRate, CurveArray& out);

} // namespace graphite::shapes
