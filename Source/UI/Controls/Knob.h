#pragma once

#include "LookAndFeel.h"

namespace graphite

{

/**
    Caption above, dial in the middle, number below - the arrangement the left
    column of every hardware EQ has used for fifty years, because it is the one
    that survives being read at a glance.

    Reachable by Tab, adjustable by arrow keys, reset by double-clicking the
    dial, editable by double-clicking the number (CONTEXT.md 9.7).
*/
class Knob final : public juce::Component
{
public:
    /** Bound to an APVTS parameter. */
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
          const juce::String& caption);

    /** Free-standing, for values that are not parameters - a fitted band's
        frequency, gain and Q are results, not automation targets. */
    Knob (const juce::String& caption, juce::Range<double> range, double interval,
          const juce::String& suffix);

    ~Knob() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    void setValue (double v, juce::NotificationType n = juce::dontSendNotification);
    double getValue() const { return slider.getValue(); }

    /** Free-standing mode only. */
    std::function<void (double)> onValueChanged;

    void setSkewForFrequency();
    void setEnabledLook (bool);

private:
    bool showValue = true;

public:

private:
    void refreshText();

    juce::String caption, suffix;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::RangedAudioParameter* parameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

} // namespace graphite
