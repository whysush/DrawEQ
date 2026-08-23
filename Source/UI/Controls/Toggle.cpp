#include "Toggle.h"

namespace draweq
{

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& parameterID,
                const juce::String& text)
    : state (s), paramID (parameterID)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setWantsKeyboardFocus (true);
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, paramID, button);
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& parameterID,
                const juce::String& text, int choiceIndex)
    : state (s), paramID (parameterID), choice (choiceIndex)
{
    button.setButtonText (text);
    button.setClickingTogglesState (false);
    button.setWantsKeyboardFocus (true);
    addAndMakeVisible (button);

    auto* parameter = state.getParameter (paramID);

    if (parameter == nullptr)
        return;

    // A segmented row is several buttons over one choice parameter, so each
    // button sets its own index and reflects whether that index is current.
    button.onClick = [this, parameter]
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (float (choice)));
        parameter->endChangeGesture();
    };

    // ParameterAttachment hands us the denormalised value and marshals to the
    // message thread itself, so this is safe to call from a host's automation
    // thread and safe to touch the button from.
    choiceAttachment = std::make_unique<juce::ParameterAttachment> (
        *parameter,
        [this] (float newValue)
        {
            button.setToggleState (int (std::lround (newValue)) == choice,
                                   juce::dontSendNotification);
        },
        nullptr);

    choiceAttachment->sendInitialUpdate();
}

Toggle::~Toggle() = default;

void Toggle::resized()
{
    button.setBounds (getLocalBounds());
}

} // namespace draweq
