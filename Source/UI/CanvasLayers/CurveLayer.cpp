#include "CurveLayer.h"

namespace draweq
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

    // Three curves, and the distinction between the first two is the thing a
    // previous version got wrong: the ghost is the hand, the target is what the
    // macros made of it, and only the target is what the DSP chases.
    const auto& targetCurve = ctx.liveTarget != nullptr ? *ctx.liveTarget : ctx.ui->targetDb;
    const auto& drawnCurve  = ctx.rawCurve != nullptr ? *ctx.rawCurve : targetCurve;

    const auto ghost    = curvePath (drawnCurve, ctx);
    const auto target   = curvePath (targetCurve, ctx);
    const auto achieved = curvePath (ctx.ui->achievedDb, ctx);

    // Only worth a line of its own when the macros actually moved something.
    bool macrosMatter = false;

    for (std::size_t i = 0; i < drawnCurve.size(); ++i)
        if (std::abs (drawnCurve[i] - targetCurve[i]) > 0.05f)
        {
            macrosMatter = true;
            break;
        }

    // --- residual ribbon --------------------------------------------------
    // The signature element. When the fit is exact it vanishes and the
    // interface is quiet; when the fit is struggling it blooms at precisely the
    // frequency where it is struggling, so the user learns the plugin's limits
    // by using it rather than by reading about them.
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

            upper.push_back ({ x, ctx.yForDb (sample (targetCurve)) });
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

    // --- the macro target, when the macros changed anything ---------------
    if (macrosMatter)
    {
        g.setColour (Theme::Colour::of (Theme::Colour::ghost).withAlpha (0.30f));
        g.strokePath (target, juce::PathStrokeType (1.0f));
    }

    // --- ghost stroke: what you drew --------------------------------------
    // Brighter while a commit is outstanding, because in that moment the ghost
    // is the only line telling the truth: the plot below it is still the old
    // filter, and the ribbon between them is the work not yet done.
    g.setColour (Theme::Colour::of (Theme::Colour::ghost)
                     .withAlpha (ctx.commitPending ? 1.0f : 0.8f));
    g.strokePath (ghost, juce::PathStrokeType (ctx.commitPending ? 2.0f : 1.5f));

    // --- plot: what you got -----------------------------------------------
    const auto plotColour = Theme::Colour::of (Theme::Colour::plot);
    g.setColour (plotColour.withAlpha (0.2f));
    g.strokePath (achieved, juce::PathStrokeType (5.0f));    // phosphor bloom
    g.setColour (plotColour);
    g.strokePath (achieved, juce::PathStrokeType (2.0f));
}

} // namespace draweq
