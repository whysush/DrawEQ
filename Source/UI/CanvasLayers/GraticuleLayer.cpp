#include "GraticuleLayer.h"

namespace draweq
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

    // Dotted rules rather than solid ones. A grid is scaffolding, and dotting
    // it keeps it legible while dropping its visual weight far enough that the
    // curve never has to compete with it - and dots are the one grid style
    // that costs nothing to land on whole pixels.
    auto dottedVertical = [&g] (int x, int top, int bottom)
    {
        for (int y = top; y < bottom; y += 3)
            g.fillRect (x, y, 1, 1);
    };

    auto dottedHorizontal = [&g] (int y, int left, int right)
    {
        for (int x = left; x < right; x += 3)
            g.fillRect (x, y, 1, 1);
    };

    for (const auto& line : verticals)
    {
        g.setColour (line.isDecade ? major : minor);
        dottedVertical (int (std::round (ctx.xForHz (line.hz))),
                        int (ctx.plot.getY()), int (ctx.plot.getBottom()));
    }

    for (int step = int (ctx.minDb / 6.0f); float (step) * 6.0f <= ctx.maxDb + 0.1f; ++step)
    {
        const float db = float (step) * 6.0f;
        const bool isZero = step == 0;

        g.setColour (isZero ? major : minor);

        // The zero line is the one rule worth drawing solid: it is the
        // reference every reading is taken against.
        if (isZero)
            g.drawHorizontalLine (int (std::round (ctx.yForDb (db))),
                                  ctx.plot.getX(), ctx.plot.getRight());
        else
            dottedHorizontal (int (std::round (ctx.yForDb (db))),
                              int (ctx.plot.getX()), int (ctx.plot.getRight()));
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

    for (float db : { -18.0f, -12.0f, -6.0f, 6.0f, 12.0f, 18.0f })
    {
        const int y = int (ctx.yForDb (db));
        g.drawText ((db > 0 ? "" : "-") + juce::String (int (std::abs (db))),
                    int (ctx.plot.getX()) + 5, y - 7, 30, 14, juce::Justification::centredLeft);
    }
}

} // namespace draweq
