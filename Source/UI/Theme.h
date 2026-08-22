#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace graphite::Theme
{

/**
    Every colour, metric and typeface in the plugin. A hex literal anywhere else
    under Source/UI is a bug (CONTEXT.md 13.3).

    The direction is a phosphor terminal: a near-black board, hairline rules, a
    single green that carries every active state, and monospaced type
    throughout. One hue does all the signalling, so brightness alone separates
    "live" from "idle" and the interface stays quiet until something is
    actually happening.

    This replaces the graphite-and-amber drafting surface described in
    CONTEXT.md 9.1-9.2; see DECISIONS.md.
*/
namespace Colour
{
    // Surfaces. Never pure black: pure black reads as a hole in a host's rack.
    inline constexpr juce::uint32 background = 0xFF050807;
    inline constexpr juce::uint32 panel      = 0xFF0A100C;
    inline constexpr juce::uint32 board      = 0xFF070C09;
    inline constexpr juce::uint32 raised     = 0xFF101A14;
    inline constexpr juce::uint32 recessed   = 0xFF060A08;

    // Structure
    inline constexpr juce::uint32 graticule  = 0xFF14201A;
    inline constexpr juce::uint32 graticuleM = 0xFF243A2C;
    inline constexpr juce::uint32 hairline   = 0xFF1B2A21;

    // The phosphor. Every active state is this colour at some brightness.
    inline constexpr juce::uint32 accent     = 0xFF3FE08A;
    inline constexpr juce::uint32 accentDim  = 0xFF1F6E45;

    // Content
    inline constexpr juce::uint32 plot       = 0xFF3FE08A;
    inline constexpr juce::uint32 ghost      = 0xFF6E8479;
    inline constexpr juce::uint32 residual   = 0x333FE08A;
    inline constexpr juce::uint32 spectrum   = 0xFF17402C;
    inline constexpr juce::uint32 spectrumPk = 0xFF2A6B48;

    // Text
    inline constexpr juce::uint32 textHi     = 0xFFC6D8CC;
    inline constexpr juce::uint32 textMid    = 0xFF7E948A;
    inline constexpr juce::uint32 textLo     = 0xFF4C5F55;

    // States. warn is the one deliberate departure from the single hue: an
    // amber against this much green is unmissable, which is the entire job of
    // the fit-error readout.
    inline constexpr juce::uint32 focus      = 0xFF3FE08A;
    inline constexpr juce::uint32 warn       = 0xFFE0A03F;

    inline juce::Colour of (juce::uint32 argb) { return juce::Colour (argb); }
}

namespace Metrics
{
    inline constexpr int defaultWidth  = 1180;
    inline constexpr int defaultHeight = 620;

    inline constexpr int slotBarHeight   = 54;   // the eight slots, across the top
    inline constexpr int canvasHeader    = 44;   // wordmark and mode buttons
    inline constexpr int toolBarHeight   = 56;   // tool glyphs and MAX ERR
    inline constexpr int freqAxisHeight  = 24;   // frequency labels, own strip
    inline constexpr int sidebarWidth    = 350;
    inline constexpr int gap             = 8;
    inline constexpr int canvasInset     = 8;

    inline constexpr float minDb = -30.0f;
    inline constexpr float maxDb =  30.0f;

    inline constexpr float labelSize  = 10.5f;
    inline constexpr float smallSize  = 12.0f;
    inline constexpr float bodySize   = 14.0f;
    inline constexpr float titleSize  = 21.0f;

    inline constexpr float tokenRadius = 9.0f;
    inline constexpr float focusRing   = 1.5f;

    /** Nothing animates for longer than this (CONTEXT.md 9.7). */
    inline constexpr int maxAnimationMs = 200;
}

/** Labels, in the monospaced face. A terminal sets everything in one width. */
juce::Font labelFont (float height = Metrics::labelSize);

/** Every number. Tabular figures, so a live readout does not jitter as its
    digits change. */
juce::Font monoFont (float height = Metrics::smallSize);

/** The wordmark, and only the wordmark. */
juce::Font titleFont();

/** Caps with tracking. JUCE has no letter-spacing attribute, so the tracking is
    applied glyph by glyph here rather than at twenty call sites. */
void drawTrackedLabel (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                       juce::Colour colour, juce::Justification justification,
                       float height = Metrics::labelSize, float trackingEm = 0.14f);

/**
    Band token colour, from the band's frequency rather than its index.

    A single-hue interface cannot ramp indigo to gold, so frequency is encoded
    as brightness along the phosphor instead: dim and desaturated at 20 Hz,
    full phosphor at 20 kHz. The property that matters survives - a glance at
    the canvas tells you where a band sits without reading a number.
*/
juce::Colour bandColour (float frequencyHz);

/** Registers the bundled typefaces. Called once by the editor. */
void loadFonts();

} // namespace graphite::Theme
