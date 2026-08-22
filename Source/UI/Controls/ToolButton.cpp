#include "ToolButton.h"
#include "ToolIcons.h"

namespace graphite
{

namespace
{
    /** Unit-box path -> button-sized path, landed on whole pixels.

        The scale factor and both offsets are integers, so the unit box maps
        onto an exact pixel rectangle: coordinate 0 is a pixel boundary and so
        is coordinate 1. Without this the icon lands on fractional pixels and
        every edge is smeared across two of them, which at 30 px is the
        difference between a pencil and a smudge. A 45 degree edge still
        antialiases - it has to - but it does so symmetrically and identically
        in every cell, instead of differently in each one. */
    juce::AffineTransform fitTo (juce::Rectangle<float> area)
    {
        const float side = std::floor (juce::jmin (area.getWidth(), area.getHeight()));
        const float x = std::round (area.getCentreX() - side * 0.5f);
        const float y = std::round (area.getCentreY() - side * 0.5f);
        return juce::AffineTransform::scale (side, side).translated (x, y);
    }
}

ToolButton::ToolButton (Tool toolToShow)
    : juce::Button (toolName (toolToShow)),
      tool (toolToShow)
{
    setWantsKeyboardFocus (true);
    setTooltip (juce::String (toolName (toolToShow)) + "  ("
                + juce::String::charToString (juce::CharacterFunctions::toUpperCase (
                      toolKey (toolToShow))) + ")");
}

void ToolButton::drawIcon (juce::Graphics& g, Tool tool, juce::Rectangle<float> area,
                           juce::Colour colour, float intensity)
{
    const auto transform = fitTo (area);
    const float side = std::floor (juce::jmin (area.getWidth(), area.getHeight()));

    // Whole-pixel stroke width, for the same reason: a 2.37 px line is two
    // grey rows, a 2 px line is two lit ones. Kept well under the narrowest
    // feature it has to outline, or the stroke closes the shape up.
    const float stroke = juce::jmax (1.0f, std::round (side * 0.070f));

    const auto strong = colour.withMultipliedAlpha (intensity);
    const auto faint  = colour.withMultipliedAlpha (intensity * 0.45f);

    auto strokePath = [&] (juce::Path p, float width, juce::Colour c)
    {
        p.applyTransform (transform);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };

    // Sharp corners: a pencil's shoulder and its flat end are hard edges, and
    // rounding them at this size eats the taper that identifies the shape.
    auto strokeSharp = [&] (juce::Path p, float width, juce::Colour c)
    {
        p.applyTransform (transform);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::butt));
    };

    auto fillPath = [&] (juce::Path p, juce::Colour c)
    {
        p.applyTransform (transform);
        g.setColour (c);
        g.fillPath (p);
    };

    switch (tool)
    {
        case Tool::pencil:
            strokeSharp (icons::pencil(), stroke, strong);
            fillPath (icons::pencilTip(), strong);
            strokeSharp (icons::pencilShoulder(), stroke * 0.8f, strong);
            // Filled, not outlined: at this size an outlined band is two lines
            // three pixels apart and reads as noise on the barrel.
            fillPath (icons::pencilFerrule(), faint);
            break;

        case Tool::line:
            strokePath (icons::line(), stroke, strong);
            fillPath (icons::lineNodes(), strong);
            break;

        case Tool::smooth:
            strokePath (icons::smooth(), stroke, strong);
            break;

        case Tool::erase:
            // Outline the block, fill only the sleeve: an all-filled rubber
            // reads as a solid lozenge and loses the two-tone cue that says
            // "eraser" rather than "some quadrilateral".
            strokePath (icons::eraseBody(), stroke, strong);
            fillPath (icons::eraseSleeve(), faint);
            strokePath (icons::eraseBaseline(), stroke, faint);
            break;

        case Tool::node:
            strokePath (icons::nodeCurve(), stroke * 0.85f, faint);
            // Punch the ring's middle out to the cell colour so it reads as a
            // hollow handle rather than a blob welded to the curve.
            fillPath (icons::nodeHandleRing(), strong);
            fillPath (icons::nodeHandleCore(), Theme::Colour::of (Theme::Colour::recessed));
            break;
    }
}

void ToolButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    g.setColour (Theme::Colour::of (on ? Theme::Colour::raised : Theme::Colour::recessed)
                     .brighter (highlighted ? 0.10f : 0.0f)
                     .darker (down ? 0.12f : 0.0f));
    g.fillRect (bounds);

    g.setColour (Theme::Colour::of (on ? Theme::Colour::accent : Theme::Colour::hairline));
    g.drawRect (bounds, on ? 1.4f : 1.0f);

    drawIcon (g, tool, bounds.reduced (bounds.getWidth() * 0.14f),
              Theme::Colour::of (on ? Theme::Colour::accent : Theme::Colour::textMid),
              on ? 1.0f : (highlighted ? 0.9f : 0.7f));

    if (hasKeyboardFocus (false))
    {
        g.setColour (Theme::Colour::of (Theme::Colour::focus));
        g.drawRect (bounds.expanded (1.0f), Theme::Metrics::focusRing);
    }
}

} // namespace graphite
