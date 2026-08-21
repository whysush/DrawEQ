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
        return { ctx.xForHz (b.freqHz), ctx.yForDb (juce::jlimit (ctx.minDb, ctx.maxDb, b.gainDb)) };
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
    g.reduceClipRegion (ctx.plot.toNearestInt().expanded (0, 20));

    for (int i = 0; i < ctx.ui->numBands; ++i)
    {
        const auto& b = ctx.ui->bands[std::size_t (i)];
        const auto centre = tokenCentre (ctx, b);
        const bool idle = std::abs (b.gainDb) < kIdleGainDb;
        const bool hot  = i == ctx.hoveredBand || i == ctx.draggedBand;

        const auto colour = Theme::bandColour (b.freqHz);
        const float radius = Theme::Metrics::tokenRadius * (hot ? 1.15f : 1.0f);
        const auto circle = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

        const float alpha = idle ? 0.25f : 1.0f;

        g.setColour (colour.withAlpha (0.25f * alpha));
        g.fillEllipse (circle);
        g.setColour (colour.withAlpha (alpha));
        g.drawEllipse (circle, hot ? 2.0f : 1.4f);

        g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
        g.setColour (Theme::Colour::of (Theme::Colour::textHi).withAlpha (alpha));
        g.drawText (juce::String (i + 1), circle.toNearestInt(), juce::Justification::centred);

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

        g.setColour (Theme::Colour::of (Theme::Colour::recessed).withAlpha (0.9f));
        g.fillRoundedRectangle (box.toFloat(), 3.0f);
        g.setColour (Theme::Colour::of (Theme::Colour::textMid));
        g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
        g.drawText (text, box, juce::Justification::centred);
    }
}

} // namespace graphite
