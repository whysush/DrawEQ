#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace graphite::Theme
{

/**
    Every colour, metric and typeface in the plugin. A hex literal anywhere else
    under Source/UI is a bug (CONTEXT.md 13.3).

    The direction is a dark instrument face drawn on a pixel grid: near-black
    chrome, a plate recessed a shade darker still, one cool blue for the
    response and one warm amber for anything the user can grab.

    Everything is square and integer-aligned. No rounded corners, no gradients,
    no bevels - the crispness comes from every edge landing on a whole pixel,
    and from bars being built out of discrete cells rather than smooth fills.
    The curve itself stays antialiased, because it is the measurement and
    stair-stepping it would make it harder to read, not more honest.

    Supersedes the earlier directions - the drafting surface of CONTEXT.md
    9.1-9.2, the phosphor terminal, and the light grey face. See DECISIONS.md.
*/
namespace Colour
{
    // The device face. Dark, faintly blue, with the plate a shade darker again
    // so it still reads as recessed into something solid.
    inline constexpr juce::uint32 background = 0xFF212428;
    inline constexpr juce::uint32 panel      = 0xFF282C31;
    inline constexpr juce::uint32 raised     = 0xFF323740;
    inline constexpr juce::uint32 recessed   = 0xFF15181B;
    inline constexpr juce::uint32 titleBar   = 0xFF16191C;

    // The plot plate, and the rules on it.
    inline constexpr juce::uint32 board      = 0xFF0F1215;
    inline constexpr juce::uint32 graticule  = 0xFF20252B;
    inline constexpr juce::uint32 graticuleM = 0xFF303740;
    inline constexpr juce::uint32 hairline   = 0xFF383D44;

    // Content. One cool blue for what the filter does, one near-white for what
    // the hand drew, so the two lines never have to be told apart by weight.
    inline constexpr juce::uint32 plot       = 0xFF57C5EF;
    inline constexpr juce::uint32 ghost      = 0xFFE4E8EC;
    inline constexpr juce::uint32 residual   = 0x3357C5EF;
    inline constexpr juce::uint32 spectrum   = 0xFF6B7079;
    inline constexpr juce::uint32 spectrumPk = 0xFF8A9099;

    // Amber: everything the user can grab or has switched on.
    inline constexpr juce::uint32 accent     = 0xFFF2A33C;
    inline constexpr juce::uint32 accentDim  = 0xFFB87C2C;

    // Bar cells: lit ones take the blue, unlit ones stay part of the chrome.
    inline constexpr juce::uint32 field      = 0xFF12262F;
    inline constexpr juce::uint32 fieldText  = 0xFF8FD9F2;

    inline constexpr juce::uint32 textHi     = 0xFFD6DAE0;
    inline constexpr juce::uint32 textMid    = 0xFF8B939C;
    inline constexpr juce::uint32 textLo     = 0xFF5A6169;
    inline constexpr juce::uint32 textOnPlate = 0xFF98A0A9;
    inline constexpr juce::uint32 titleText  = 0xFFE8EBEF;

    inline constexpr juce::uint32 focus      = 0xFF57C5EF;
    inline constexpr juce::uint32 warn       = 0xFFE86A3C;

    inline juce::Colour of (juce::uint32 argb) { return juce::Colour (argb); }
}

namespace Metrics
{
    inline constexpr int defaultWidth  = 880;
    inline constexpr int defaultHeight = 400;

    inline constexpr int titleBarHeight   = 22;
    inline constexpr int rightColumnWidth = 142;
    inline constexpr int bottomStripHeight = 54;   // two rows of controls
    inline constexpr int rowHeight        = 22;
    inline constexpr int freqAxisHeight   = 0;     // labels sit inside the plate
    inline constexpr int gap              = 4;
    inline constexpr int canvasInset      = 3;

    /** The plate shows +/-24 dB rather than the model's full +/-30. Drawing is
        bounded by what is on screen, so nothing is hidden by the choice - it
        only means a shape loaded from a preset can have its last few dB at the
        extreme edges of the spectrum drawn flat against the rail. In return the
        curve occupies a useful fraction of the plate instead of a third of it. */
    inline constexpr float minDb = -24.0f;
    inline constexpr float maxDb =  24.0f;

    inline constexpr float labelSize  = 10.0f;
    inline constexpr float smallSize  = 11.0f;
    inline constexpr float bodySize   = 12.0f;
    inline constexpr float titleSize  = 13.0f;

    inline constexpr float tokenRadius = 8.0f;
    inline constexpr float focusRing   = 1.0f;

    /** Bar cells are drawn this wide with a one pixel gap, which is what makes
        a bar read as counted rather than poured. */
    inline constexpr int barCellWidth = 4;
    inline constexpr int barCellGap   = 1;

    /** Nothing animates for longer than this (CONTEXT.md 9.7). */
    inline constexpr int maxAnimationMs = 200;
}

/** Section captions and button text. */
juce::Font labelFont (float height = Metrics::labelSize);

/** Every number. Tabular figures, so a live readout does not jitter as its
    digits change (CONTEXT.md 9.3). */
juce::Font monoFont (float height = Metrics::smallSize);

juce::Font titleFont();

/** Caps with tracking, applied glyph by glyph because JUCE has no
    letter-spacing attribute. */
void drawTrackedLabel (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                       juce::Colour colour, juce::Justification justification,
                       float height = Metrics::labelSize, float trackingEm = 0.08f);

/**
    Band token colour, from the band's frequency rather than its index.

    All of them are amber, because amber is what "you can grab this" means
    everywhere else on the panel. Frequency rides along it as a warm-to-bright
    ramp - deep ochre at 20 Hz, pale gold at 20 kHz - so a glance still says
    roughly where a band sits without the strip turning into a rainbow.
*/
juce::Colour bandColour (float frequencyHz);

/** Registers the bundled typefaces. Called once by the editor. */
void loadFonts();

} // namespace graphite::Theme
