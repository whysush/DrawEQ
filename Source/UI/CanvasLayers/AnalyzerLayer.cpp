#include "AnalyzerLayer.h"

namespace draweq
{

namespace
{
    /** The analyser reads in absolute dBFS; the canvas is a relative dB scale.
        Anchoring -18 dBFS to the 0 dB line puts a normal mix in the middle of
        the plot without any auto-ranging for the eye to chase. */
    constexpr float kReference = -18.0f;

    juce::Path buildPath (const std::array<float, Analyzer::kPoints>& data,
                          const CanvasContext& ctx, bool close)
    {
        juce::Path path;
        bool started = false;

        for (int i = 0; i < Analyzer::kPoints; ++i)
        {
            const float hz = Analyzer::pointToHz (i);
            const float x  = ctx.xForHz (hz);
            const float y  = ctx.yForDb (juce::jlimit (ctx.minDb, ctx.maxDb,
                                                       data[std::size_t (i)] - kReference));

            if (! started)
            {
                if (close)
                {
                    path.startNewSubPath (x, ctx.plot.getBottom());
                    path.lineTo (x, y);
                }
                else
                {
                    path.startNewSubPath (x, y);
                }

                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        if (close && started)
        {
            path.lineTo (ctx.plot.getRight(), ctx.plot.getBottom());
            path.closeSubPath();
        }

        return path;
    }
}

void AnalyzerLayer::paint (juce::Graphics& g, const CanvasContext& ctx)
{
    if (! ctx.analyzerOn || ctx.analyzer == nullptr)
        return;

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (ctx.plot.toNearestInt());

    // Post as a fill, pre as an outline: the filled one is the result, and the
    // result is what the eye should land on first.
    const auto fill = Theme::Colour::of (Theme::Colour::spectrum).withAlpha (0.55f);
    g.setColour (fill);
    g.fillPath (buildPath (ctx.analyzer->postDb(), ctx, true));

    g.setColour (Theme::Colour::of (Theme::Colour::spectrumPk).withAlpha (0.45f));
    g.strokePath (buildPath (ctx.analyzer->postPeakDb(), ctx, false), juce::PathStrokeType (1.0f));

    g.setColour (Theme::Colour::of (Theme::Colour::spectrum).withAlpha (0.85f));
    g.strokePath (buildPath (ctx.analyzer->preDb(), ctx, false), juce::PathStrokeType (1.0f));
}

} // namespace draweq
