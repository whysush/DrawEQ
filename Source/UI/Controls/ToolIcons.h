#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace graphite::icons
{

/**
    Tool glyphs as vector paths, drawn in a unit box and scaled by the caller.

    Not font characters. The obvious route - setting the button text to a
    pencil or an erase glyph - depends on whichever fallback face the host
    machine happens to have, so the icons would differ between two users'
    screens and one of them would get tofu. Paths render identically
    everywhere, take the theme colour, and stay crisp at every UI scale.

    Each builder fills the unit square [0, 1]^2. Strokes are described in the
    same units, so the caller scales the stroke width alongside the path.
*/

/**
    Pencil.

    Built from one axis rather than drawn by eye, so every edge is derived and
    the pieces share exact vertices instead of nearly meeting. The silhouette is
    a half-width profile swept along a 45 degree axis:

        t = 0.00   half width 0        the point
        t = 0.17   half width 0.062    graphite meets wood
        t = 0.33   half width 0.140    the shoulder reaches full barrel
        t = 1.00   half width 0.140    the flat end

    So the outline tapers to an actual point, which is what the previous version
    was missing: it was an untapered parallelogram with two detached triangles
    floating near it, and at 30 px that reads as a blob rather than a pencil.
*/
namespace detail
{
    struct PencilAxis
    {
        // 45 degrees, tip at lower left. Chosen so the whole silhouette lands
        // inside the unit box with a hair of margin at both extremes.
        static constexpr float tipX   = 0.150f;
        static constexpr float tipY   = 0.850f;
        static constexpr float dirX   =  0.70710678f;
        static constexpr float dirY   = -0.70710678f;
        static constexpr float perpX  =  0.70710678f;
        static constexpr float perpY  =  0.70710678f;
        static constexpr float length = 1.0000f;

        static constexpr float graphiteEnd  = 0.17f;
        static constexpr float shoulderEnd  = 0.33f;
        static constexpr float ferruleFrom  = 0.68f;
        static constexpr float ferruleTo    = 0.80f;
        // Wide enough that a two pixel outline still leaves an open interior.
        // At the previous 0.105 the barrel was six pixels across and a three
        // pixel stroke closed it up into a solid stick.
        static constexpr float halfWidth    = 0.140f;
        static constexpr float graphiteHalf = 0.062f;

        /** Point at distance `t` along the axis (0 = tip, 1 = end), offset by
            `halfW` across it. Every vertex in every pencil path comes from
            here, which is what keeps the pieces registered to each other. */
        static juce::Point<float> at (float t, float halfW) noexcept
        {
            const float along = t * length;
            return { tipX + dirX * along + perpX * halfW,
                     tipY + dirY * along + perpY * halfW };
        }
    };
}

/** The full outline: point, graphite flank, shoulder, barrel, flat end. */
inline juce::Path pencil()
{
    using A = detail::PencilAxis;

    juce::Path p;
    p.startNewSubPath (A::at (0.0f, 0.0f));
    p.lineTo (A::at (A::graphiteEnd,  A::graphiteHalf));
    p.lineTo (A::at (A::shoulderEnd,  A::halfWidth));
    p.lineTo (A::at (1.0f,            A::halfWidth));
    p.lineTo (A::at (1.0f,           -A::halfWidth));
    p.lineTo (A::at (A::shoulderEnd, -A::halfWidth));
    p.lineTo (A::at (A::graphiteEnd, -A::graphiteHalf));
    p.closeSubPath();
    return p;
}

/** The exposed graphite: the cone from the point to the wood line. Filled, so
    the business end is unmistakably the business end. */
inline juce::Path pencilTip()
{
    using A = detail::PencilAxis;

    juce::Path p;
    p.startNewSubPath (A::at (0.0f, 0.0f));
    p.lineTo (A::at (A::graphiteEnd,  A::graphiteHalf));
    p.lineTo (A::at (A::graphiteEnd, -A::graphiteHalf));
    p.closeSubPath();
    return p;
}

/** The ferrule band across the barrel. Sharing the barrel's half width means
    its ends sit exactly on the outline rather than just inside or just past it. */
inline juce::Path pencilFerrule()
{
    using A = detail::PencilAxis;

    juce::Path p;
    p.startNewSubPath (A::at (A::ferruleFrom,  A::halfWidth));
    p.lineTo (A::at (A::ferruleTo,    A::halfWidth));
    p.lineTo (A::at (A::ferruleTo,   -A::halfWidth));
    p.lineTo (A::at (A::ferruleFrom, -A::halfWidth));
    p.closeSubPath();
    return p;
}

/** The wood line where the sharpening stops - the shoulder, drawn across. */
inline juce::Path pencilShoulder()
{
    using A = detail::PencilAxis;

    juce::Path p;
    p.startNewSubPath (A::at (A::shoulderEnd,  A::halfWidth));
    p.lineTo (A::at (A::shoulderEnd, -A::halfWidth));
    return p;
}

/** Line: a diagonal with terminal nodes, so it cannot be mistaken for the
    pencil's barrel. The nodes also say what the tool does - it is anchored at
    two points, not freehand. */
inline juce::Path line()
{
    juce::Path p;
    p.startNewSubPath (0.18f, 0.82f);
    p.lineTo (0.82f, 0.18f);
    return p;
}

inline juce::Path lineNodes()
{
    juce::Path p;
    p.addEllipse (0.06f, 0.70f, 0.22f, 0.22f);
    p.addEllipse (0.72f, 0.06f, 0.22f, 0.22f);
    return p;
}

/** Smooth: a wave that starts jagged and ends rounded. A plain tilde says
    "wave"; this says "this makes the wave calmer", which is the actual verb. */
inline juce::Path smooth()
{
    juce::Path p;
    p.startNewSubPath (0.08f, 0.46f);
    p.lineTo (0.20f, 0.18f);
    p.lineTo (0.32f, 0.80f);
    p.lineTo (0.42f, 0.36f);
    // ...and out the other side as a settled curve. Three spikes read as noise
    // at 30 px; two is enough to say "before", and the tail says "after".
    p.cubicTo (0.60f, 0.36f, 0.66f, 0.62f, 0.92f, 0.60f);
    return p;
}

/** Erase: a rubber block, angled, with a sleeve band and the surface it is
    clearing.

    Two details do the work. The blunt end - an eraser has no point, and that
    is the single strongest cue separating it from the pencil. And the baseline
    with a cleared gap under it, which says what the block is for rather than
    leaving it as an anonymous quadrilateral. */
inline juce::Path eraseBody()
{
    juce::Path p;
    p.startNewSubPath (0.30f, 0.66f);
    p.lineTo (0.66f, 0.22f);
    p.lineTo (0.90f, 0.42f);
    p.lineTo (0.54f, 0.86f);
    p.closeSubPath();
    return p;
}

/** The sleeve: the darker half of the rubber, nearer the working face. */
inline juce::Path eraseSleeve()
{
    juce::Path p;
    p.startNewSubPath (0.30f, 0.66f);
    p.lineTo (0.42f, 0.52f);
    p.lineTo (0.66f, 0.72f);
    p.lineTo (0.54f, 0.86f);
    p.closeSubPath();
    return p;
}

/** The line being erased, with a gap where the block has passed. */
inline juce::Path eraseBaseline()
{
    juce::Path p;
    p.startNewSubPath (0.08f, 0.90f);
    p.lineTo (0.28f, 0.90f);
    return p;
}

/** Node: a curve with one handle sitting on it, which is literally the
    gesture - grab a band on the response and move it. */
inline juce::Path nodeCurve()
{
    // Flatter than a hump, so the handle sitting on it is what the eye lands
    // on rather than the peak.
    juce::Path p;
    p.startNewSubPath (0.06f, 0.72f);
    p.cubicTo (0.26f, 0.72f, 0.30f, 0.40f, 0.50f, 0.40f);
    p.cubicTo (0.70f, 0.40f, 0.74f, 0.60f, 0.94f, 0.58f);
    return p;
}

/** The handle is a ring, not a dot, because that is exactly what a band token
    on the canvas looks like - the icon and the thing it manipulates are the
    same shape. */
inline juce::Path nodeHandleRing()
{
    juce::Path p;
    p.addEllipse (0.34f, 0.24f, 0.32f, 0.32f);
    return p;
}

inline juce::Path nodeHandleCore()
{
    juce::Path p;
    p.addEllipse (0.42f, 0.32f, 0.16f, 0.16f);
    return p;
}

} // namespace graphite::icons
