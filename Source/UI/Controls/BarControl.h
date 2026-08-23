#pragma once

#include "LookAndFeel.h"

namespace graphite
{

/**
    `TILT  ####------  0` - a caption, a bar built from discrete cells, a number.

    One control type for every value on the panel, which is most of what makes
    the layout tight: a bar is a fifth the height of a dial and reads at a
    glance from further away.

    The cells are the point. A poured bar has to be measured against its ends to
    be read; a counted one can be read directly, and it lands on whole pixels by
    construction, which is what keeps the panel crisp at any UI scale.

    Drag to change, double-click to reset, double-click the number to type.
*/
class BarControl final : public juce::Component
{
public:
    /** Bound to an APVTS parameter. */
    BarControl (juce::AudioProcessorValueTreeState&, const juce::String& parameterID,
                const juce::String& caption, int captionWidth = 44);

    /** Free-standing, for values that are results rather than automation
        targets - a fitted band's frequency, gain and Q. */
    BarControl (const juce::String& caption, juce::Range<double> range, double interval,
                const juce::String& suffix, int captionWidth = 44);

    ~BarControl() override;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarControl)
};

} // namespace graphite
