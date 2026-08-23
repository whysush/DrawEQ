#include "GraticuleLayer.h"

namespace graphite
{

void GraticuleLayer::paint (juce::Graphics& g, const CanvasContext& ctx)
{
    const auto minor = Theme::Colour::of (Theme::Colour::graticule);
    const auto major = Theme::Colour::of (Theme::Colour::graticuleM);

    struct Line { float hz; bool isDecade; };

    static constexpr Line verticals[] {
        {    20.0f, false }, {    30.0f, false }, {    50.0f, false },
        {   100.0f, true  }, {   200.0f, false }, {   300.0f, false }, { 500.0f, false },
        {  1000.0f, true  }, {  2000.0f, false }, {  3000.0f, false }, { 5000.0f, false },
        { 10000.0f, true  }, { 20000.0f, false }
    };

    for (const auto& line : verticals)
    {
        const float x = ctx.xForHz (line.hz);

        g.setColour (line.isDecade ? major : minor);
        g.drawVerticalLine (int (std::round (x)), ctx.plot.getY(), ctx.plot.getBottom());
    }

    for (int step = int (ctx.minDb / 6.0f); float (step) * 6.0f <= ctx.maxDb + 0.1f; ++step)
    {
        const float db = float (step) * 6.0f;
        const float y = ctx.yForDb (db);
        const bool isZero = step == 0;

        g.setColour (isZero ? major : minor);
        g.drawHorizontalLine (int (std::round (y)), ctx.plot.getX(), ctx.plot.getRight());
    }

    // The frequency scale gets its own ruled strip below the plot rather than
    // floating over it. Inside the plot they would eventually sit under the
    // curve, and a label the curve crosses is worse than no label.
    // Both scales are set inside the plate, in a dim version of the plate's own
    // text colour. Outside it they would need a margin the plot could otherwise
    // be using, and at this dimness they never compete with the curve.
    g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
    g.setColour (Theme::Colour::of (Theme::Colour::textOnPlate).withAlpha (0.55f));

    for (float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        const juce::String text = hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k"
                                                : juce::String (int (hz));
        const int x = int (ctx.xForHz (hz));
        g.drawText (text, x - 20, int (ctx.plot.getBottom()) - 17, 40, 14,
                    juce::Justification::centred);
    }

    for (float db : { -12.0f, -6.0f, 6.0f, 12.0f })
    {
        const int y = int (ctx.yForDb (db));
        g.drawText ((db > 0 ? "" : "-") + juce::String (int (std::abs (db))),
                    int (ctx.plot.getX()) + 5, y - 7, 30, 14, juce::Justification::centredLeft);
    }
}

} // namespace graphite
