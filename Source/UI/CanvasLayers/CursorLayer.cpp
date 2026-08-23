#include "CursorLayer.h"

namespace draweq
{

void CursorLayer::paint (juce::Graphics& g, const CanvasContext& ctx, juce::Point<float> mouse,
                         bool visible, float brushOctaves, bool showBrush)
{
    if (! visible || ! ctx.plot.contains (mouse))
        return;

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (ctx.plot.toNearestInt());

    g.setColour (Theme::Colour::of (Theme::Colour::focus).withAlpha (0.35f));
    g.drawVerticalLine (int (mouse.x), ctx.plot.getY(), ctx.plot.getBottom());
    g.drawHorizontalLine (int (mouse.y), ctx.plot.getX(), ctx.plot.getRight());

    if (showBrush)
    {
        // The brush is a width in octaves, so it is drawn as one: the same
        // gesture covers the same span of pixels wherever the cursor is, and
        // seeing that is what makes the behaviour learnable.
        const float hz = ctx.hzForX (mouse.x);
        const float x0 = ctx.xForHz (hz * std::pow (2.0f, -brushOctaves));
        const float x1 = ctx.xForHz (hz * std::pow (2.0f,  brushOctaves));

        g.setColour (Theme::Colour::of (Theme::Colour::focus).withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float> (x0, mouse.y - 1.0f, x1 - x0, 2.0f));
    }

    const float hz = ctx.hzForX (mouse.x);
    const float db = ctx.dbForY (mouse.y);

    const juce::String line1 = (hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                                              : juce::String (hz, 0) + " Hz")
                             + "  " + noteNameFor (hz);
    const juce::String line2 = (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB";

    const int w = 116, h = 30;
    // Flip to the other side of the cursor near the edges so the readout is
    // never clipped or drawn off the plot.
    const bool flipX = mouse.x + w + 16 > ctx.plot.getRight();
    const bool flipY = mouse.y - h - 16 < ctx.plot.getY();

    auto box = juce::Rectangle<int> (w, h)
                        .withPosition (int (flipX ? mouse.x - w - 10 : mouse.x + 10),
                                       int (flipY ? mouse.y + 10 : mouse.y - h - 10));

    g.setColour (Theme::Colour::of (Theme::Colour::recessed).withAlpha (0.88f));
    g.fillRoundedRectangle (box.toFloat(), 3.0f);
    g.setColour (Theme::Colour::of (Theme::Colour::hairline));
    g.drawRoundedRectangle (box.toFloat(), 3.0f, 1.0f);

    g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
    g.setColour (Theme::Colour::of (Theme::Colour::textHi));
    g.drawText (line1, box.removeFromTop (15).reduced (6, 0), juce::Justification::centredLeft);
    g.setColour (Theme::Colour::of (Theme::Colour::textMid));
    g.drawText (line2, box.reduced (6, 0), juce::Justification::centredLeft);
}

void CursorLayer::paintErrorReadout (juce::Graphics& g, const CanvasContext& ctx)
{
    if (ctx.ui == nullptr || ! ctx.ui->valid)
        return;

    // The number itself lives in the tool bar. What belongs on the canvas is
    // the offer, and only when it applies: quiet, non-modal, and pointing at
    // the mode that would fit this exactly (CONTEXT.md 7.5).
    if (ctx.ui->maxErrorDb <= 3.0f || ctx.mode != Mode::analog)
        return;

    // Clear of the frequency scale along the bottom of the plate, which shares
    // this corner.
    g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
    g.setColour (Theme::Colour::of (Theme::Colour::warn).brighter (0.4f));
    g.drawText ("Spectral mode will fit this exactly",
                int (ctx.plot.getX()) + 8, int (ctx.plot.getBottom()) - 38, 340, 14,
                juce::Justification::centredLeft);
}

} // namespace draweq
