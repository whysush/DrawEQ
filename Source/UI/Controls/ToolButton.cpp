#include "ToolButton.h"
#include "ToolIcons.h"

namespace graphite
{

namespace
{
    /** Unit-box path -> button-sized path. Stroke widths are given in the same
        unit space and scaled here too, so an icon keeps its weight at any UI
        scale. */
    juce::AffineTransform fitTo (juce::Rectangle<float> area)
    {
        const float side = juce::jmin (area.getWidth(), area.getHeight());
        return juce::AffineTransform::scale (side, side)
                   .translated (area.getCentreX() - side * 0.5f,
                                area.getCentreY() - side * 0.5f);
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
    const float side = juce::jmin (area.getWidth(), area.getHeight());
    const float stroke = juce::jmax (1.4f, side * 0.088f);

    const auto strong = colour.withMultipliedAlpha (intensity);
    const auto faint  = colour.withMultipliedAlpha (intensity * 0.45f);

    auto strokePath = [&] (juce::Path p, float width, juce::Colour c)
    {
        p.applyTransform (transform);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
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
            strokePath (icons::pencil(), stroke, strong);
            fillPath (icons::pencilTip(), strong);
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
