#include "CurveLayer.h"

namespace graphite
{

namespace
{
    juce::Path curvePath (const CurveArray& curve, const CanvasContext& ctx)
    {
        juce::Path path;

        // One point per pixel column rather than per grid bin: at 1024 bins
        // across 900 px the extra points cost time and change nothing.
        const int columns = juce::jmax (2, int (ctx.plot.getWidth()));

        for (int c = 0; c < columns; ++c)
        {
            const float x   = ctx.plot.getX() + float (c);
            const float idx = LogGrid::clampIndex (LogGrid::hzToIndex (ctx.hzForX (x)));
            const int   i0  = int (idx);
            const int   i1  = juce::jmin (i0 + 1, LogGrid::kSize - 1);
            const float db  = curve[std::size_t (i0)]
                            + (idx - float (i0)) * (curve[std::size_t (i1)] - curve[std::size_t (i0)]);
            const float y   = ctx.yForDb (db);

            if (c == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        return path;
    }
}

void CurveLayer::paint (juce::Graphics& g, const CanvasContext& ctx)
{
    if (ctx.ui == nullptr || ! ctx.ui->valid)
        return;

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (ctx.plot.toNearestInt());

    const auto ghost    = curvePath (ctx.ui->targetDb, ctx);
    const auto achieved = curvePath (ctx.ui->achievedDb, ctx);

    // --- residual ribbon --------------------------------------------------
    //
    // The signature element. When the fit is exact it vanishes and the
    // interface is quiet; when the fit is struggling it blooms at precisely the
    // frequency where it is struggling, so the user learns the plugin's limits
    // by using it rather than by reading about them (CONTEXT.md 9.4).
    {
        // The closed band between the two lines: out along the ghost, back
        // along the plot.
        juce::Path filled;
        const int columns = juce::jmax (2, int (ctx.plot.getWidth()));
        std::vector<juce::Point<float>> upper, lower;
        upper.reserve (std::size_t (columns));
        lower.reserve (std::size_t (columns));

        for (int c = 0; c < columns; ++c)
        {
            const float x   = ctx.plot.getX() + float (c);
            const float idx = LogGrid::clampIndex (LogGrid::hzToIndex (ctx.hzForX (x)));
            const int   i0  = int (idx);
            const int   i1  = juce::jmin (i0 + 1, LogGrid::kSize - 1);
            const float t   = idx - float (i0);

            auto sample = [&] (const CurveArray& a)
            {
                return a[std::size_t (i0)] + t * (a[std::size_t (i1)] - a[std::size_t (i0)]);
            };

            upper.push_back ({ x, ctx.yForDb (sample (ctx.ui->targetDb)) });
            lower.push_back ({ x, ctx.yForDb (sample (ctx.ui->achievedDb)) });
        }

        filled.startNewSubPath (upper.front());

        for (const auto& p : upper)
            filled.lineTo (p);

        for (auto it = lower.rbegin(); it != lower.rend(); ++it)
            filled.lineTo (*it);

        filled.closeSubPath();

        g.setColour (Theme::Colour::of (Theme::Colour::residual));
        g.fillPath (filled);
    }

    // --- ghost stroke: what you drew --------------------------------------
    g.setColour (Theme::Colour::of (Theme::Colour::ghost).withAlpha (0.7f));
    g.strokePath (ghost, juce::PathStrokeType (1.5f));

    // --- plot: what you got -----------------------------------------------
    const auto plotColour = Theme::Colour::of (Theme::Colour::plot);
    g.setColour (plotColour.withAlpha (0.2f));
    g.strokePath (achieved, juce::PathStrokeType (5.0f));    // phosphor bloom
    g.setColour (plotColour);
    g.strokePath (achieved, juce::PathStrokeType (2.0f));
}

} // namespace graphite
