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

/** Pencil: hexagonal barrel, a collar, a tapered shoulder, and a graphite tip.

    The collar is what makes it legible at 20 px. Without it the shape reads as
    a plain wedge, which is the flaw in most small pencil icons - and the reason
    a pencil and an eraser drawn as bare quadrilaterals look like each other. */
inline juce::Path pencil()
{
    juce::Path p;

    // Barrel, running from lower-left to upper-right at 45 degrees.
    p.startNewSubPath (0.16f, 0.84f);   // tip
    p.lineTo (0.10f, 0.90f);            // the point's underside
    p.lineTo (0.16f, 0.96f);
    p.closeSubPath();

    juce::Path barrel;
    barrel.startNewSubPath (0.20f, 0.80f);
    barrel.lineTo (0.72f, 0.28f);
    barrel.lineTo (0.86f, 0.42f);
    barrel.lineTo (0.34f, 0.94f);
    barrel.closeSubPath();
    p.addPath (barrel);

    // Collar: the ferrule band, a short bar across the barrel near the top.
    juce::Path collar;
    collar.startNewSubPath (0.64f, 0.20f);
    collar.lineTo (0.78f, 0.34f);
    collar.lineTo (0.72f, 0.40f);
    collar.lineTo (0.58f, 0.26f);
    collar.closeSubPath();
    p.addPath (collar);

    return p;
}

/** The pencil's graphite tip, drawn filled so the point reads as the business
    end rather than as an empty corner. */
inline juce::Path pencilTip()
{
    juce::Path p;
    p.startNewSubPath (0.20f, 0.80f);
    p.lineTo (0.34f, 0.94f);
    p.lineTo (0.16f, 0.98f);
    p.closeSubPath();
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
