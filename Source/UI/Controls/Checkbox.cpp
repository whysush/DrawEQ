#include "Checkbox.h"

namespace graphite
{

class Checkbox::Box final : public juce::Button
{
public:
    explicit Box (const juce::String& caption) : juce::Button (caption)
    {
        setClickingTogglesState (true);
        setWantsKeyboardFocus (true);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto bounds = getLocalBounds();
        const int side = juce::jmin (14, bounds.getHeight() - 4);
        const auto square = juce::Rectangle<int> (side, side)
                                .withY (bounds.getCentreY() - side / 2)
                                .withX (bounds.getX())
                                .toFloat().reduced (0.5f);

        g.setColour (Theme::Colour::of (Theme::Colour::recessed));
        g.fillRect (square);
        g.setColour (Theme::Colour::of (getToggleState() ? Theme::Colour::accent
                                                         : Theme::Colour::hairline));
        g.drawRect (square, 1.0f);

        if (getToggleState())
        {
            // A filled core rather than a tick: at 14 px a tick is three grey
            // pixels and a solid block is unambiguous.
            g.setColour (Theme::Colour::of (Theme::Colour::accent));
            g.fillRect (square.reduced (3.5f));
        }

        Theme::drawTrackedLabel (g, getButtonText(),
                                 bounds.withTrimmedLeft (side + 10),
                                 Theme::Colour::of (getToggleState() ? Theme::Colour::textHi
                                          : highlighted ? Theme::Colour::textMid
                                                        : Theme::Colour::textLo),
                                 juce::Justification::centredLeft);

        if (hasKeyboardFocus (false))
        {
            g.setColour (Theme::Colour::of (Theme::Colour::focus));
            g.drawRect (bounds.toFloat().expanded (-0.5f), Theme::Metrics::focusRing);
        }
    }
};

Checkbox::Checkbox (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                    const juce::String& label)
{
    box = std::make_unique<Box> (label);
    addAndMakeVisible (*box);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, parameterID, *box);
}

Checkbox::~Checkbox() = default;

void Checkbox::resized()
{
    box->setBounds (getLocalBounds());
}

} // namespace graphite
