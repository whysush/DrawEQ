#pragma once

#include "Controls/Checkbox.h"
#include "Controls/Console.h"
#include "Controls/Knob.h"
#include "Controls/SlotStrip.h"
#include "Controls/ToolButton.h"
#include "Controls/ValueField.h"
#include "CurveCanvas.h"

namespace graphite
{

class GraphiteProcessor;

/**
    A grey instrument face: title strip across the top, the selected band's
    controls down the left, settings down the right, tools and slots along the
    bottom, and the plot plate taking everything that is left.

    The plate is the instrument and the chrome is the settings for it, so the
    plate gets every pixel the chrome does not need.
*/
class GraphiteEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit GraphiteEditor (GraphiteProcessor&);
    ~GraphiteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshToolButtons();
    void refreshBandColumn();
    void pushBandEdit();
    void toggleAnalyser();
    int  analyserChoice() const;

    GraphiteProcessor& processor;
    GraphiteLookAndFeel lookAndFeel;

    CurveCanvas canvas;
    SlotStrip   slots;
    Console     status;

    // Left column: whichever band the Node tool has hold of.
    Knob bandFreq { "Freq", { 20.0, 20000.0 }, 0.1, "Hz" };
    Knob bandGain { "Gain", { -30.0, 30.0 }, 0.01, "dB" };
    Knob bandQ    { "Q",    { 0.1, 18.0 }, 0.01, "" };
    bool suppressBandCallback = false;

    std::array<std::unique_ptr<ToolButton>, 5> toolButtons;

    juce::ComboBox modeBox, fidelityBox, shapeBox, analyserBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment,
                                                                           analyserAttachment;

    ValueField bands, mix, output, tilt, smooth, shift, morph;
    Checkbox   invert, bypass, live;
    juce::TextButton analyseButton { "Analyse" };

    // Painted regions, kept so live readouts can repaint without the canvas.
    juce::Rectangle<int> titleArea, leftColumn, rightColumn, bottomStrip, plateArea;

    /** Captions for the controls that do not draw their own. Collected during
        layout so paint() has nowhere to disagree with resized() about where
        they go. */
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    int lastAnalyserChoice = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphiteEditor)
};

} // namespace graphite
