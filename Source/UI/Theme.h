#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace graphite::Theme
{

/**
    Every colour, metric and typeface in the plugin. A hex literal anywhere else
    under Source/UI is a bug (CONTEXT.md 13.3).

    The direction is a drafting surface rather than the genre-standard blue-grey
    plate: an ink-dark board, a hairline graticule, a graphite stroke where the
    hand went, and a warm plotter line showing what the machine produced. The
    two lines being different is the signature.
*/
namespace Colour
{
    // Surfaces - deep blue-black, never pure black: pure black reads as a hole
    // in every host's mixer.
    inline constexpr juce::uint32 board      = 0xFF0E1116;
    inline constexpr juce::uint32 panel      = 0xFF151A21;
    inline constexpr juce::uint32 raised     = 0xFF1D242D;
    inline constexpr juce::uint32 recessed   = 0xFF0A0D11;

    // Structure
    inline constexpr juce::uint32 graticule  = 0xFF232B35;
    inline constexpr juce::uint32 graticuleM = 0xFF313B47;
    inline constexpr juce::uint32 hairline   = 0xFF2A333F;

    // Content
    inline constexpr juce::uint32 ghost      = 0xFF7C8794;
    inline constexpr juce::uint32 plot       = 0xFFE8C46A;
    inline constexpr juce::uint32 residual   = 0x33E8C46A;
    inline constexpr juce::uint32 spectrum   = 0xFF2F4A5C;
    inline constexpr juce::uint32 spectrumPk = 0xFF48708A;

    // Text
    inline constexpr juce::uint32 textHi     = 0xFFD6DDE5;
    inline constexpr juce::uint32 textMid    = 0xFF8A95A2;
    inline constexpr juce::uint32 textLo     = 0xFF576372;

    // States
    inline constexpr juce::uint32 focus      = 0xFF6FA8C7;
    inline constexpr juce::uint32 warn       = 0xFFC77B5A;

    inline juce::Colour of (juce::uint32 argb) { return juce::Colour (argb); }
}

namespace Metrics
{
    inline constexpr int headerHeight = 40;
    inline constexpr int footerHeight = 72;
    inline constexpr int canvasInset  = 10;
    inline constexpr int defaultWidth = 1000;
    inline constexpr int defaultHeight = 560;

    inline constexpr float minDb = -30.0f;
    inline constexpr float maxDb =  30.0f;

    inline constexpr float labelSize  = 10.5f;
    inline constexpr float smallSize  = 12.0f;
    inline constexpr float bodySize   = 14.0f;
    inline constexpr float titleSize  = 20.0f;

    inline constexpr float tokenRadius = 9.0f;   // 18 px circles
    inline constexpr float focusRing   = 1.5f;

    /** Nothing animates for longer than this (CONTEXT.md 9.7). */
    inline constexpr int maxAnimationMs = 200;
}

/** Section labels, mode names, buttons: caps, tracked out. */
juce::Font labelFont (float height = Metrics::labelSize);

/** Every number in the plugin. Tabular figures, so a live readout does not
    jitter as its digits change. */
juce::Font monoFont (float height = Metrics::smallSize);

juce::Font titleFont();

/** Draws caps-with-tracking text; JUCE has no letter-spacing attribute, so the
    tracking is applied by drawing glyph by glyph. */
void drawTrackedLabel (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                       juce::Colour colour, juce::Justification justification,
                       float height = Metrics::labelSize, float trackingEm = 0.08f);

/**
    Band token colour, from the band's frequency rather than its index.

    Deliberately not a rainbow: a single perceptual path from deep indigo at
    20 Hz through slate and sage to pale gold at 20 kHz. Because the ramp is
    driven by frequency, the colour encodes something true, and a glance at the
    strip tells the user where the bands sit without reading a number.
*/
juce::Colour bandColour (float frequencyHz);

/** Registers the bundled typefaces. Called once by the editor. */
void loadFonts();

} // namespace graphite::Theme
