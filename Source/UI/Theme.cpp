#include "Theme.h"
#include "../Core/LogGrid.h"

#if DRAWEQ_HAS_RESOURCES
 #include "BinaryData.h"
#endif

namespace draweq::Theme
{

namespace
{
    juce::Typeface::Ptr grotesk, mono;

    juce::Font makeFont (juce::Typeface::Ptr face, float height, bool fallbackMono)
    {
        if (face != nullptr)
            return juce::Font (juce::FontOptions (face).withHeight (height));

        // A missing bundled font must not be a crash or a blank interface; the
        // nearest system face keeps the plugin usable while being obviously not
        // the intended one.
        return juce::Font (juce::FontOptions (fallbackMono ? juce::Font::getDefaultMonospacedFontName()
                                                           : juce::Font::getDefaultSansSerifFontName(),
                                              height, juce::Font::plain));
    }
}

void loadFonts()
{
   #if DRAWEQ_HAS_RESOURCES
    if (grotesk == nullptr)
        grotesk = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskMedium_ttf,
                                                           BinaryData::SpaceGroteskMedium_ttfSize);

    if (mono == nullptr)
        mono = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoRegular_ttf,
                                                        BinaryData::JetBrainsMonoRegular_ttfSize);
   #endif
}

// Monospaced labels: at 10 px on a pixel grid it is the face whose stems
// land on whole pixels, and the wordmark is the only thing set in the grotesk.
juce::Font labelFont (float height) { return makeFont (mono, height, true); }
juce::Font monoFont  (float height) { return makeFont (mono, height, true); }
juce::Font titleFont()              { return makeFont (grotesk, Metrics::titleSize, false); }

void drawTrackedLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                       juce::Colour colour, juce::Justification justification,
                       float height, float trackingEm)
{
    const auto upper = text.toUpperCase();
    const auto font  = labelFont (height);
    const float tracking = height * trackingEm;

    juce::GlyphArrangement arrangement;
    arrangement.addLineOfText (font, upper, 0.0f, 0.0f);

    // Push each glyph right by the accumulated tracking. Doing it here rather
    // than at every call site is what keeps the labels consistent.
    float shift = 0.0f;
    float total = 0.0f;

    for (int i = 0; i < arrangement.getNumGlyphs(); ++i)
    {
        auto& glyph = arrangement.getGlyph (i);
        const auto bounds = glyph.getBounds();
        total = bounds.getRight() + shift;
        glyph.moveBy (shift, 0.0f);
        shift += tracking;
    }

    const float width = total;
    float x = float (area.getX());

    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = float (area.getCentreX()) - width * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = float (area.getRight()) - width;

    const float y = float (area.getCentreY()) + height * 0.35f;

    g.setColour (colour);
    arrangement.draw (g, juce::AffineTransform::translation (x, y));
}

void drawPixelBevel (juce::Graphics& g, juce::Rectangle<int> r, bool raised)
{
    if (r.getWidth() < 2 || r.getHeight() < 2)
        return;

    const auto light = Colour::of (raised ? Colour::bevelLight : Colour::bevelDark);
    const auto dark  = Colour::of (raised ? Colour::bevelDark : Colour::bevelLight);

    g.setColour (light);
    g.fillRect (r.getX(), r.getY(), r.getWidth() - 1, 1);
    g.fillRect (r.getX(), r.getY(), 1, r.getHeight() - 1);

    g.setColour (dark);
    g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);
    g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
}

juce::Colour bandColour (float frequencyHz)
{
    // Stops along one brightness path, placed on the same log axis the canvas
    // uses, so a token's colour and its position agree.
    struct Stop { float norm; juce::uint32 argb; };

    static constexpr Stop stops[] {
        { 0.00f, 0xFFD98A2B },   // deep ochre, 20 Hz
        { 0.35f, 0xFFE8992F },
        { 0.65f, 0xFFF2A33C },
        { 0.85f, 0xFFF5B94A },
        { 1.00f, 0xFFF7CC5C }    // pale gold, 20 kHz
    };

    const float t = juce::jlimit (0.0f, 1.0f, LogGrid::hzToNorm (frequencyHz));

    for (std::size_t i = 1; i < std::size (stops); ++i)
    {
        if (t <= stops[i].norm)
        {
            const auto& a = stops[i - 1];
            const auto& b = stops[i];
            const float local = (t - a.norm) / juce::jmax (1.0e-6f, b.norm - a.norm);
            return juce::Colour (a.argb).interpolatedWith (juce::Colour (b.argb), local);
        }
    }

    return juce::Colour (stops[std::size (stops) - 1].argb);
}

} // namespace draweq::Theme
