#pragma once

#include "LogGrid.h"
#include <array>
#include <cstdint>

namespace draweq
{

using CurveArray = std::array<float, LogGrid::kSize>;

enum class Mode : int
{
    spectralLinear = 0,
    spectralMinimum,
    analog
};

/**
    A trivially copyable copy of everything the worker needs to build a
    FilterState. It crosses the message->worker boundary by value, which is why
    it holds no pointers, no strings and nothing that owns memory.
*/
struct CurveSnapshot
{
    CurveArray raw {};            // exactly what the user drew, pre-macros
    std::uint64_t version = 0;    // bumps on every edit; worker skips duplicates

    // Macros are captured with the curve so the worker always sees a set that
    // was true at the same instant. Reading them separately would let a fit
    // land halfway between two states.
    float tiltDbPerDecade    = 0.0f;
    float smoothOctaves      = 0.0f;
    float freqShiftSemitones = 0.0f;

    // Morph is how a drawing becomes automatable: a 1024-point curve is not a
    // parameter any host will accept, but the interpolation between the live
    // curve and a stored slot is a single float (CONTEXT.md 8.1).
    CurveArray morphTarget {};
    float      morphAmount = 0.0f;

    int    bandCount  = 12;
    Mode   mode       = Mode::analog;
    double sampleRate = 48000.0;
};

} // namespace draweq
