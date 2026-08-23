#pragma once

#include "../../Core/LogGrid.h"
#include "../../DSP/Analyzer.h"
#include "../../DSP/CurveWorker.h"
#include "../Theme.h"

namespace draweq
{

/**
    Everything the canvas layers need in order to draw, and the one place that
    maps frequency and dB onto pixels.

    Layers are plain objects rather than Components: they all paint into the
    same rectangle in a fixed order, and giving each one a Component would buy
    nothing but compositing overhead and z-order bugs.
*/
struct CanvasContext
{
    juce::Rectangle<float> plot;
    juce::Rectangle<float> axis;   // frequency labels, in their own strip below

    float minDb = Theme::Metrics::minDb;
    float maxDb = Theme::Metrics::maxDb;

    const CurveWorker::UiSnapshot* ui = nullptr;
    const Analyzer* analyzer = nullptr;

    /** Exactly what the pencil wrote, with no macros applied. This is the ghost
        line, and it must pass under the cursor: a drawing tool whose line does
        not land where you put it is broken, whatever the reason. */
    const CurveArray* rawCurve = nullptr;

    /** The same stroke after tilt, shift and smoothing - what the DSP is
        actually asked for. Drawn separately and faintly, because it is a
        consequence of the macros rather than of the user's hand. The worker's
        own snapshot only updates on commit, so while a stroke is in progress
        this is the only thing that knows what is being drawn. */
    const CurveArray* liveTarget = nullptr;

    /** A stroke has been drawn but not yet realised by the DSP. */
    bool commitPending = false;

    Mode mode = Mode::analog;
    bool analyzerOn = true;
    int  hoveredBand = -1;
    int  draggedBand = -1;

    float xForHz (float hz) const noexcept
    {
        return plot.getX() + LogGrid::hzToNorm (hz) * plot.getWidth();
    }

    float hzForX (float x) const noexcept
    {
        return LogGrid::normToHz ((x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
    }

    float yForDb (float db) const noexcept
    {
        const float t = (db - minDb) / (maxDb - minDb);
        return plot.getBottom() - juce::jlimit (0.0f, 1.0f, t) * plot.getHeight();
    }

    float dbForY (float y) const noexcept
    {
        const float t = (plot.getBottom() - y) / juce::jmax (1.0f, plot.getHeight());
        return minDb + t * (maxDb - minDb);
    }

    float xForGridIndex (int i) const noexcept { return xForHz (LogGrid::indexToHz (float (i))); }
};

/** Nearest note name for a frequency, for the cursor readout. Musicians think
    in notes at the bottom of the range far more than in hertz. */
inline juce::String noteNameFor (float hz)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    if (hz <= 0.0f)
        return {};

    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int   note = int (std::lround (midi));
    const int   octave = note / 12 - 1;

    return juce::String (names[((note % 12) + 12) % 12]) + juce::String (octave);
}

} // namespace draweq
