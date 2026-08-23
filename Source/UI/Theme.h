#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace graphite::Theme
{

/**
    Every colour, metric and typeface in the plugin. A hex literal anywhere else
    under Source/UI is a bug (CONTEXT.md 13.3).

    The direction is a grey instrument face: warm neutral chrome, a plot plate
    recessed into it in dark slate, one cool blue for the response and one warm
    amber for anything the user can grab. Old and new at once - the panel reads
    as a physical device, the controls on it are flat and modern, and nothing is
    textured or bevelled for its own sake.

    Supersedes both earlier directions - the graphite-and-amber drafting surface
    of CONTEXT.md 9.1-9.2 and the phosphor terminal that replaced it. See
    DECISIONS.md.
*/
namespace Colour
{
    // The device face. Mid grey, slightly warm, so the plate reads as recessed
    // into something solid rather than floating on black.
    inline constexpr juce::uint32 background = 0xFF8F9195;
    inline constexpr juce::uint32 panel      = 0xFF9A9CA0;
    inline constexpr juce::uint32 raised     = 0xFFAAACB0;
    inline constexpr juce::uint32 recessed   = 0xFF7C7E82;
    inline constexpr juce::uint32 titleBar   = 0xFF6E7074;

    // The plot plate, and the rules on it.
    inline constexpr juce::uint32 board      = 0xFF383D44;
    inline constexpr juce::uint32 graticule  = 0xFF454B53;
    inline constexpr juce::uint32 graticuleM = 0xFF565D66;
    inline constexpr juce::uint32 hairline   = 0xFF6A6C70;

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

    // Value fields, which are tinted rather than outlined.
    inline constexpr juce::uint32 field      = 0xFFA9DFF2;
    inline constexpr juce::uint32 fieldText  = 0xFF1B2126;

    // Text on the grey chrome is dark; text on the plate is light.
    inline constexpr juce::uint32 textHi     = 0xFF1E2024;
    inline constexpr juce::uint32 textMid    = 0xFF3E4146;
    inline constexpr juce::uint32 textLo     = 0xFF5E6166;
    inline constexpr juce::uint32 textOnPlate = 0xFFB6BCC4;
    inline constexpr juce::uint32 titleText  = 0xFFF2F3F5;

    inline constexpr juce::uint32 focus      = 0xFF34ABE0;
    inline constexpr juce::uint32 warn       = 0xFFD9552F;

    inline juce::Colour of (juce::uint32 argb) { return juce::Colour (argb); }
}

namespace Metrics
{
    inline constexpr int defaultWidth  = 1040;
    inline constexpr int defaultHeight = 500;

    inline constexpr int titleBarHeight   = 30;
    inline constexpr int leftColumnWidth  = 92;   // Freq / Gain / Q for the selected band
    inline constexpr int rightColumnWidth = 132;
    inline constexpr int bottomStripHeight = 62;
    inline constexpr int freqAxisHeight   = 0;    // labels sit inside the plate
    inline constexpr int gap              = 6;
    inline constexpr int canvasInset      = 4;

    inline constexpr float minDb = -30.0f;
    inline constexpr float maxDb =  30.0f;

    inline constexpr float labelSize  = 11.0f;
    inline constexpr float smallSize  = 12.0f;
    inline constexpr float bodySize   = 13.0f;
    inline constexpr float titleSize  = 16.0f;

    inline constexpr float tokenRadius = 10.0f;
    inline constexpr float focusRing   = 1.5f;

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
                       float height = Metrics::labelSize, float trackingEm = 0.04f);

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
