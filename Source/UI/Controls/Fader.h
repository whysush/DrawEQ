#pragma once

#include "LookAndFeel.h"

namespace draweq
{

/**
    A console fader: a recessed slot with ticks along it, and a capped handle
    running in it.

    The cap is what a progress bar could never be. A filled bar tells you how
    much; a cap tells you *where*, and where is the question being asked of a
    tilt or a shift. It also gives the eye something to aim at, which is the
    difference between a control you drag and a control you nudge.

    One control type for every value on the panel, so the layout stays tight and
    nothing has to be learned twice. Drag to move, double-click to reset,
    double-click the number to type.
*/
class Fader final : public juce::Component
{
public:
    /** Bound to an APVTS parameter. */
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
                const juce::String& caption, int captionWidth = 44);

    /** Free-standing, for values that are results rather than automation
        targets - a fitted band's frequency, gain and Q. */
    Fader (const juce::String& caption, juce::Range<double> range, double interval,
                const juce::String& suffix, int captionWidth = 44);

    ~Fader() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    void setValue (double v, juce::NotificationType = juce::dontSendNotification);
    double getValue() const { return slider.getValue(); }

    void setSkewForFrequency();
    void setEnabledLook (bool);
    bool isBeingDragged() const { return slider.isMouseButtonDown(); }

    std::function<void (double)> onValueChanged;

private:
    void refreshText();

    juce::String caption, suffix;
    int captionWidth = 44;
    bool showValue = true;

    juce::Slider slider { juce::Slider::LinearBar, juce::Slider::NoTextBox };
    juce::Label value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::RangedAudioParameter* parameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fader)
};

} // namespace draweq
