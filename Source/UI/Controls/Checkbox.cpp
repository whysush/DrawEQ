#include "Checkbox.h"

namespace draweq
{

class Checkbox::Box final : public juce::Button
{
public:
    explicit Box (const juce::String& caption) : juce::Button (caption)
    {
        setClickingTogglesState (true);
        setWantsKeyboardFocus (true);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto bounds = getLocalBounds();
        const bool on = getToggleState();

        // Amber when on, grey when off - the same statement every other switch
        // on the panel makes, so none of them need explaining twice.
        g.setColour (Theme::Colour::of (on ? Theme::Colour::accent : Theme::Colour::raised)
                         .brighter (highlighted ? 0.08f : 0.0f)
                         .darker (down ? 0.10f : 0.0f));
        g.fillRect (bounds);

        Theme::drawPixelBevel (g, bounds, ! on);

        Theme::drawTrackedLabel (g, getButtonText(), getLocalBounds(),
                                 Theme::Colour::of (on ? Theme::Colour::textHi
                                                       : Theme::Colour::textMid),
                                 juce::Justification::centred);

        if (hasKeyboardFocus (false))
        {
            g.setColour (Theme::Colour::of (Theme::Colour::focus));
            g.drawRect (bounds, int (Theme::Metrics::focusRing));
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

} // namespace draweq
