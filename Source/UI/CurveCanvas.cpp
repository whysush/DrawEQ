#include "CurveCanvas.h"
#include "../Processor.h"
#include "CanvasLayers/AnalyzerLayer.h"
#include "CanvasLayers/CurveLayer.h"
#include "CanvasLayers/CursorLayer.h"
#include "CanvasLayers/GraticuleLayer.h"
#include "../Core/CurveShaping.h"

namespace draweq
{

CurveCanvas::CurveCanvas (DrawEQProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (true);
   #if ! JUCE_LINUX
    // A crosshair is the right pointer for a surface you draw on, and it is set
    // everywhere it can safely be set.
    //
    // Not on Linux. JUCE caches the standard X cursor handles beyond the life
    // of the X display, so asking for one here makes XCloseDisplay dereference
    // freed state during teardown: pluginval segfaults on every run, and the
    // crash bisects to exactly this call - a canvas with no cursor, no timer
    // and no children passes, and adding this line back fails. Nothing in the
    // plugin's own memory is at fault; AddressSanitizer is silent right up to
    // the segfault inside libX11.
    //
    // Windows is the v1 target (CONTEXT.md 2) and is unaffected, and the canvas
    // draws its own crosshair hairlines anyway (CursorLayer), so Linux loses
    // nothing visible. Worth revisiting when Linux becomes a real target.
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
   #endif
    startTimerHz (60);
    lastUpdateSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

CurveCanvas::~CurveCanvas()
{
    stopTimer();
}

void CurveCanvas::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float elapsed = float (now - lastUpdateSeconds);
    lastUpdateSeconds = now;

    if (processor.analyzer().anyEnabled())
        processor.analyzer().update (elapsed);

    ui = processor.worker().uiSnapshot();

    // Recomputed every frame through the same macro chain the worker uses, so
    // the ghost cannot drift from the curve the DSP will be asked for.
    CurveSnapshot snap;
    processor.worker().fillCurrentSnapshot (snap);
    rawCurve = snap.raw;
    shaping::applyMacros (snap, liveTarget);

    repaint();
}

CanvasContext CurveCanvas::makeContext() const
{
    CanvasContext ctx;

    // The scales are drawn inside the plate now, so the plot takes all of it.
    ctx.plot = getLocalBounds().toFloat().reduced (float (Theme::Metrics::canvasInset));
    ctx.ui         = &ui;
    ctx.analyzer   = &processor.analyzer();
    ctx.mode       = ui.mode;
    ctx.analyzerOn = processor.analyzer().anyEnabled();
    ctx.rawCurve      = &rawCurve;
    ctx.liveTarget    = &liveTarget;
    ctx.commitPending = processor.worker().commitPending();
    ctx.hoveredBand = current == Tool::node && mouseInside
                    ? BandTokenLayer::hitTest (ctx, mousePos) : -1;
    ctx.draggedBand = grabbedBand >= 0 ? grabbedBand : chosenBand;
    return ctx;
}

void CurveCanvas::paint (juce::Graphics& g)
{
    const auto ctx = makeContext();

    g.fillAll (Theme::Colour::of (Theme::Colour::board));

    GraticuleLayer::paint (g, ctx);
    AnalyzerLayer::paint (g, ctx);
    CurveLayer::paint (g, ctx);
    BandTokenLayer::paint (g, ctx);

    if (linePreview)
    {
        g.setColour (Theme::Colour::of (Theme::Colour::ghost).withAlpha (0.5f));
        g.drawLine ({ lineFrom, lineTo }, 1.0f);
    }

    CursorLayer::paint (g, ctx, mousePos, mouseInside, brushOctaves,
                        current != Tool::node && current != Tool::line);
    CursorLayer::paintErrorReadout (g, ctx);

    g.setColour (Theme::Colour::of (Theme::Colour::hairline));
    g.drawRect (ctx.plot, 1.0f);

    if (hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus).withAlpha (0.5f));
        g.drawRect (getLocalBounds(), int (Theme::Metrics::focusRing));
    }
}

void CurveCanvas::resized() {}

void CurveCanvas::setTool (Tool t)
{
    if (current == t)
        return;

    current = t;
    grabbedBand = -1;

    if (t != Tool::node)
    {
        chosenBand = -1;

        if (onBandSelectionChanged != nullptr)
            onBandSelectionChanged();
    }

    if (onToolChanged != nullptr)
        onToolChanged();

    repaint();
}

Tool CurveCanvas::effectiveTool (const juce::MouseEvent& e) const
{
    // Modifiers are consistent across every tool: Alt is always "smooth for a
    // moment", a right-drag is always "pull toward flat".
    if (e.mods.isAltDown())
        return Tool::smooth;

    if (e.mods.isRightButtonDown())
        return Tool::erase;

    return current;
}

void CurveCanvas::applyStroke (const juce::MouseEvent& e, bool first)
{
    const auto ctx = makeContext();
    const auto pos = e.position;

    const float hz = juce::jlimit (LogGrid::kFMin, LogGrid::kFMax, ctx.hzForX (pos.x));
    float db = juce::jlimit (ctx.minDb, ctx.maxDb, ctx.dbForY (pos.y));

    if (lockDb)
        db = lockedDb;

    // Ctrl is fine adjust everywhere, which for a brush means less pressure per
    // pass rather than a smaller brush.
    const float pressure = e.mods.isCommandDown() ? 0.25f : 1.0f;

    if (first)
        processor.curve().startStroke (hz, db);

    processor.curve().strokeTo (hz, db, brushOctaves, pressure, brushFor (effectiveTool (e)));
}

void CurveCanvas::dragBand (const juce::MouseEvent& e)
{
    if (grabbedBand < 0 || grabbedBand >= editableCount)
        return;

    const auto ctx = makeContext();
    auto& band = editableBands[std::size_t (grabbedBand)];

    const float sensitivity = e.mods.isCommandDown() ? 0.25f : 1.0f;

    const float targetHz = juce::jlimit (LogGrid::kFMin, LogGrid::kFMax, ctx.hzForX (e.position.x));
    const float targetDb = juce::jlimit (ctx.minDb, ctx.maxDb, ctx.dbForY (e.position.y));

    band.freqHz = std::exp (std::log (band.freqHz)
                            + sensitivity * (std::log (targetHz) - std::log (band.freqHz)));

    if (! e.mods.isShiftDown())   // Shift locks the level, as everywhere else
        band.gainDb = band.gainDb + sensitivity * (targetDb - band.gainDb);

    commitBands();
}

void CurveCanvas::commitBands()
{
    CurveArray derived {};
    curveFromBands (editableBands.data(), editableCount, editableTrim,
                    processor.getSampleRate(), derived);
    processor.curve().setCurve (derived);
}

void CurveCanvas::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    const auto ctx = makeContext();

    if (current == Tool::node)
    {
        grabbedBand = BandTokenLayer::hitTest (ctx, e.position);

        if (grabbedBand != chosenBand)
        {
            chosenBand = grabbedBand;

            if (onBandSelectionChanged != nullptr)
                onBandSelectionChanged();
        }

        if (grabbedBand >= 0)
        {
            editableBands = ui.bands;
            editableCount = ui.numBands;
            editableTrim  = ui.trimDb;
            processor.curve().beginGesture();
            dragging = true;
        }

        return;
    }

    dragging = true;
    lockDb   = e.mods.isShiftDown();
    lockedDb = juce::jlimit (ctx.minDb, ctx.maxDb, ctx.dbForY (e.position.y));

    processor.curve().beginGesture();

    if (current == Tool::line)
    {
        linePreview = true;
        lineFrom = lineTo = e.position;
        return;
    }

    applyStroke (e, true);
}

void CurveCanvas::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    mouseInside = true;

    if (! dragging)
        return;

    if (current == Tool::node)
    {
        dragBand (e);
        return;
    }

    if (current == Tool::line)
    {
        lineTo = e.position;
        repaint();
        return;
    }

    applyStroke (e, false);
}

void CurveCanvas::mouseUp (const juce::MouseEvent& e)
{
    if (dragging && current == Tool::line && linePreview)
    {
        const auto ctx = makeContext();
        processor.curve().drawLine (ctx.hzForX (lineFrom.x), ctx.dbForY (lineFrom.y),
                                    ctx.hzForX (e.position.x),
                                    lockDb ? lockedDb : ctx.dbForY (e.position.y),
                                    brushOctaves);
    }

    if (dragging)
        processor.curve().endGesture();

    // A hand-edited band stack is not a small perturbation of the previous fit,
    // so the next one starts cold rather than warm.
    if (current == Tool::node && grabbedBand >= 0)
        processor.worker().requestDeepFit();

    dragging = false;
    linePreview = false;
    lockDb = false;
    grabbedBand = -1;

    if (onBandSelectionChanged != nullptr)
        onBandSelectionChanged();

    repaint();
}

bool CurveCanvas::selectedBand (Band& out) const
{
    if (chosenBand < 0 || chosenBand >= ui.numBands)
        return false;

    out = ui.bands[std::size_t (chosenBand)];
    return true;
}

void CurveCanvas::updateSelectedBand (float freqHz, float gainDb, float q)
{
    if (chosenBand < 0 || chosenBand >= ui.numBands)
        return;

    editableBands = ui.bands;
    editableCount = ui.numBands;
    editableTrim  = ui.trimDb;

    auto& band = editableBands[std::size_t (chosenBand)];
    band.freqHz = juce::jlimit (LogGrid::kFMin, LogGrid::kFMax, freqHz);
    band.gainDb = juce::jlimit (-LogGrid::kMaxDb, LogGrid::kMaxDb, gainDb);
    band.q      = juce::jlimit (0.1f, 18.0f, q);

    processor.curve().beginGesture();
    commitBands();
    processor.curve().endGesture();
    processor.worker().requestDeepFit();
}

void CurveCanvas::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    mouseInside = true;
    repaint();
}

void CurveCanvas::mouseExit (const juce::MouseEvent&)
{
    mouseInside = false;
    repaint();
}

void CurveCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto ctx = makeContext();
    processor.curve().flattenWindow (ctx.hzForX (e.position.x), 1.0f);
}

void CurveCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (current == Tool::node)
    {
        const auto ctx = makeContext();
        const int over = BandTokenLayer::hitTest (ctx, e.position);

        if (over >= 0 && over < ui.numBands)
        {
            // Wheel over a token is Q: the third dimension of a band, and the
            // one there is no room for on a two-axis canvas.
            editableBands = ui.bands;
            editableCount = ui.numBands;
            editableTrim  = ui.trimDb;

            auto& band = editableBands[std::size_t (over)];
            band.q = juce::jlimit (0.1f, 18.0f, band.q * std::exp (wheel.deltaY * 2.0f));

            processor.curve().beginGesture();
            commitBands();
            processor.curve().endGesture();
            processor.worker().requestDeepFit();
            return;
        }
    }

    brushOctaves = juce::jlimit (0.05f, 3.0f, brushOctaves * std::exp (wheel.deltaY * 1.5f));
    repaint();
}

bool CurveCanvas::keyPressed (const juce::KeyPress& key)
{
    for (Tool t : { Tool::pencil, Tool::line, Tool::smooth, Tool::erase, Tool::node })
        if (key.isKeyCode (juce::CharacterFunctions::toUpperCase (toolKey (t))))
        {
            setTool (t);
            return true;
        }

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        processor.curve().undo();
        processor.worker().requestDeepFit();
        return true;
    }

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier
                                    | juce::ModifierKeys::shiftModifier, 0))
    {
        processor.curve().redo();
        processor.worker().requestDeepFit();
        return true;
    }

    return false;
}

} // namespace draweq
