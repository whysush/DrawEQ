#include "StickerLayer.h"

namespace draweq
{

void StickerLayer::paint (juce::Graphics& g, const CanvasContext& ctx, const juce::Image& image)
{
    if (! image.isValid() || ctx.plot.getWidth() < 260.0f || ctx.plot.getHeight() < 180.0f)
        return;   // no room to be charming on a very small window

    // Sized against the plate rather than fixed, so he keeps his proportion of
    // the panel at every UI scale.
    const int height = juce::jlimit (58, 104, int (ctx.plot.getHeight() * 0.24f));
    const int width  = juce::roundToInt (float (height) * float (image.getWidth())
                                                        / float (image.getHeight()));

    // Bottom right, clear of the frequency scale along the bottom edge.
    const auto frame = juce::Rectangle<int> (width, height)
                           .withRightX (int (ctx.plot.getRight()) - 10)
                           .withBottomY (int (ctx.plot.getBottom()) - 24);

    g.setColour (Theme::Colour::of (Theme::Colour::recessed));
    g.fillRect (frame.expanded (3));

    g.drawImage (image, frame.toFloat(), juce::RectanglePlacement::stretchToFit);

    // The same one-pixel frame every raised thing on the panel wears, so he
    // belongs to the interface rather than sitting on top of it.
    Theme::drawPixelBevel (g, frame.expanded (3), true);
}

}
