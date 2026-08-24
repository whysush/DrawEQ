#pragma once

#include "CanvasLayers/BandTokenLayer.h"
#include "CanvasLayers/CanvasContext.h"
#include "Tools/DrawTools.h"

namespace draweq
{

class DrawEQProcessor;

/**
    The drawing surface: every layer, every tool, every gesture.

    Coordinates only ever leave this class as (frequency, dB). The model is
    never told about pixels, which is what lets the same curve mean the same
    thing at any window size (CONTEXT.md 6.1).
*/
class CurveCanvas final : public juce::Component,
                          private juce::Timer
{
public:
    explicit CurveCanvas (DrawEQProcessor&);
    ~CurveCanvas() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool keyPressed (const juce::KeyPress&) override;

    void setTool (Tool t);
    Tool tool() const noexcept { return current; }

    std::function<void()> onToolChanged;

    /** Fires when the band the left column is editing changes. */
    std::function<void()> onBandSelectionChanged;

    /** The band currently under edit, if any. */
    bool selectedBand (Band& out) const;

    /** Writes new values into the selected band and re-derives the curve from
        the whole stack - the same path the Node tool's drag takes, so a number
        typed into the left column and a token dragged on the canvas cannot
        disagree. */
    void updateSelectedBand (float freqHz, float gainDb, float q);

private:
    void timerCallback() override;
    CanvasContext makeContext() const;
    Tool effectiveTool (const juce::MouseEvent&) const;
    void applyStroke (const juce::MouseEvent&, bool first);
    void dragBand (const juce::MouseEvent&);
    void commitBands();

    DrawEQProcessor& processor;
    CurveWorker::UiSnapshot ui;
    CurveArray liveTarget {}, rawCurve {};
    juce::Image sticker;

    Tool current = Tool::pencil;
    float brushOctaves = 0.5f;

    bool  dragging = false;
    bool  lockDb = false;
    float lockedDb = 0.0f;
    juce::Point<float> mousePos;
    bool  mouseInside = false;

    // Line tool preview, live only while the button is down.
    bool  linePreview = false;
    juce::Point<float> lineFrom, lineTo;

    // Node tool
    int   grabbedBand = -1;
    int   chosenBand = -1;
    std::array<Band, kMaxBands> editableBands {};
    int   editableCount = 0;
    float editableTrim = 0.0f;

    double lastUpdateSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveCanvas)
};

} // namespace draweq
