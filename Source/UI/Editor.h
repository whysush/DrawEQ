#pragma once

#include "Controls/Checkbox.h"
#include "Controls/Console.h"
#include "Controls/BarControl.h"
#include "Controls/SlotStrip.h"
#include "Controls/ToolButton.h"
#include "CurveCanvas.h"

namespace graphite
{

class GraphiteProcessor;

/**
    A dark instrument face: a thin title strip, settings down the right, two
    rows of controls along the bottom, and the plot plate taking everything
    left over.

    Every control is the same segmented bar, which is most of what makes this
    fit in 880 by 400: a bar is a fifth the height of a dial, so the panel needs
    two short rows where it used to need a column down each side.
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

    // Whichever band the Node tool has hold of, edited numerically.
    BarControl bandFreq { "Freq", { 20.0, 20000.0 }, 0.1, "Hz", 34 };
    BarControl bandGain { "Gain", { -30.0, 30.0 }, 0.01, "dB", 34 };
    BarControl bandQ    { "Q",    { 0.1, 18.0 }, 0.01, "", 34 };
    bool suppressBandCallback = false;

    std::array<std::unique_ptr<ToolButton>, 5> toolButtons;

    juce::ComboBox modeBox, fidelityBox, shapeBox, analyserBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment,
                                                                           analyserAttachment;

    BarControl bands, mix, output, tilt, smooth, shift, morph;
    Checkbox   invert, bypass, live;
    juce::TextButton analyseButton { "Analyse" };

    // Painted regions, kept so live readouts can repaint without the canvas.
    juce::Rectangle<int> titleArea, rightColumn, bottomStrip, plateArea, bandArea;

    /** Captions for the controls that do not draw their own. Collected during
        layout so paint() has nowhere to disagree with resized() about where
        they go. */
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    int lastAnalyserChoice = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphiteEditor)
};

} // namespace graphite
