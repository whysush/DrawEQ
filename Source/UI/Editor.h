#pragma once

#include "Controls/Knob.h"
#include "Controls/SlotStrip.h"
#include "Controls/Toggle.h"
#include "CurveCanvas.h"

namespace graphite
{

class GraphiteProcessor;

/**
    Header, canvas, footer. The canvas takes every pixel the chrome does not
    need, because the canvas is the instrument and the chrome is the settings
    for it (CONTEXT.md 9.5).
*/
class GraphiteEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GraphiteEditor (GraphiteProcessor&);
    ~GraphiteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshToolButtons();

    GraphiteProcessor& processor;
    GraphiteLookAndFeel lookAndFeel;

    CurveCanvas canvas;
    SlotStrip   slots;

    std::array<juce::TextButton, 5> toolButtons;
    std::array<std::unique_ptr<Toggle>, 3> modeButtons;

    Knob morph, tilt, smooth, shift, bands, mix, output;

    juce::ComboBox analyzerBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> analyzerAttachment;

    Toggle invert, bypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphiteEditor)
};

} // namespace graphite
