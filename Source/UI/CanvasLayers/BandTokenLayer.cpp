#include "BandTokenLayer.h"

namespace graphite
{

namespace
{
    /** A band at 0 dB has no visible effect, so its token would be a handle for
        nothing. They are still drawn, but faintly, so the user can grab one and
        recruit it. */
    constexpr float kIdleGainDb = 0.25f;

    juce::Point<float> tokenCentre (const CanvasContext& ctx, const Band& b)
    {
        // Held far enough inside the plate that the whole disc stays on it. A
        // band at 20 kHz is still at 20 kHz; a handle sliced in half by the
        // plate edge is just harder to grab.
        const float inset = Theme::Metrics::tokenRadius + 2.0f;

        return { juce::jlimit (ctx.plot.getX() + inset, ctx.plot.getRight() - inset,
                               ctx.xForHz (b.freqHz)),
                 juce::jlimit (ctx.plot.getY() + inset, ctx.plot.getBottom() - inset,
                               ctx.yForDb (juce::jlimit (ctx.minDb, ctx.maxDb, b.gainDb))) };
    }
}

int BandTokenLayer::hitTest (const CanvasContext& ctx, juce::Point<float> p)
{
    if (ctx.ui == nullptr || ctx.mode != Mode::analog)
        return -1;

    int   best = -1;
    float bestDistance = Theme::Metrics::tokenRadius * 2.0f;

    for (int i = 0; i < ctx.ui->numBands; ++i)
    {
        const float d = tokenCentre (ctx, ctx.ui->bands[std::size_t (i)]).getDistanceFrom (p);

        if (d < bestDistance)
        {
            bestDistance = d;
            best = i;
        }
    }

    return best;
}

void BandTokenLayer::paint (juce::Graphics& g, const CanvasContext& ctx)
{
    if (ctx.ui == nullptr || ! ctx.ui->valid || ctx.mode != Mode::analog)
        return;

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (ctx.plot.toNearestInt());

    for (int i = 0; i < ctx.ui->numBands; ++i)
    {
        const auto& b = ctx.ui->bands[std::size_t (i)];
        const auto centre = tokenCentre (ctx, b);
        const bool idle = std::abs (b.gainDb) < kIdleGainDb;
        const bool hot  = i == ctx.hoveredBand || i == ctx.draggedBand;

        const auto colour = Theme::bandColour (b.freqHz);
        const int side = int (Theme::Metrics::tokenRadius * 2.0f) + (hot ? 2 : 0);

        // Square handles, landed on whole pixels. Solid rather than outlined:
        // against a dark plate a filled handle is unambiguously a thing to
        // grab, and the number stays readable where an outline and its digit
        // would start to merge.
        const auto cell = juce::Rectangle<int> (side, side)
                              .withCentre ({ int (std::round (centre.x)),
                                             int (std::round (centre.y)) });

        const float alpha = idle ? 0.4f : 1.0f;

        g.setColour (colour.withAlpha (alpha));
        g.fillRect (cell);

        if (hot)
        {
            g.setColour (Theme::Colour::of (Theme::Colour::titleText));
            g.drawRect (cell.expanded (2), 1);
        }

        g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
        g.setColour (Theme::Colour::of (Theme::Colour::board).withAlpha (alpha));
        g.drawText (juce::String (i + 1), cell, juce::Justification::centred);

        if (! hot)
            continue;

        // Hovering expands the token into its numbers - the shelves say so
        // rather than showing a Q that means something different for them.
        const juce::String type = b.type == BandType::lowShelf  ? "LS"
                                : b.type == BandType::highShelf ? "HS" : "PK";

        const juce::String text = type + "  "
                                + (b.freqHz >= 1000.0f ? juce::String (b.freqHz / 1000.0f, 2) + "k"
                                                       : juce::String (b.freqHz, 0))
                                + "  " + juce::String (b.gainDb, 1) + "dB"
                                + "  Q" + juce::String (b.q, 2);

        const auto box = juce::Rectangle<int> (150, 18)
                            .withCentre ({ int (centre.x), int (centre.y) - 24 })
                            .constrainedWithin (ctx.plot.toNearestInt());

        g.setColour (Theme::Colour::of (Theme::Colour::board).withAlpha (0.94f));
        g.fillRect (box);
        g.setColour (Theme::Colour::of (Theme::Colour::hairline));
        g.drawRect (box, 1);
        g.setColour (Theme::Colour::of (Theme::Colour::textOnPlate));
        g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
        g.drawText (text, box, juce::Justification::centred);
    }
}

} // namespace graphite
