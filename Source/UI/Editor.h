#pragma once

#include "Controls/Checkbox.h"
#include "Controls/Console.h"
#include "Controls/Fader.h"
#include "Controls/SlotStrip.h"
#include "Controls/ToolButton.h"
#include "CurveCanvas.h"

namespace draweq
{

class DrawEQProcessor;

/**
    A dark instrument face: a thin title strip, settings down the right, two
    rows of controls along the bottom, and the plot plate taking everything
    left over.

    Every control is the same fader, so nothing has to be learned twice and the
    panel needs two short rows along the bottom where it would otherwise need a
    column down each side.
*/
class DrawEQEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit DrawEQEditor (DrawEQProcessor&);
    ~DrawEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshToolButtons();
    void refreshBandColumn();
    void pushBandEdit();
    void toggleAnalyser();
    int  analyserChoice() const;

    DrawEQProcessor& processor;
    DrawEQLookAndFeel lookAndFeel;

    CurveCanvas canvas;
    SlotStrip   slots;
    Console     status;

    // Whichever band the Node tool has hold of, edited numerically.
    Fader bandFreq { "Freq", { 20.0, 20000.0 }, 0.1, "Hz", 34 };
    Fader bandGain { "Gain", { -30.0, 30.0 }, 0.01, "dB", 34 };
    Fader bandQ    { "Q",    { 0.1, 18.0 }, 0.01, "", 34 };
    bool suppressBandCallback = false;

    std::array<std::unique_ptr<ToolButton>, 5> toolButtons;

    juce::ComboBox modeBox, fidelityBox, shapeBox, analyserBox, toneBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment,
                                                                           analyserAttachment,
                                                                           toneAttachment;

    Fader bands, mix, output, tilt, smooth, shift, morph, toneFreq;
    Checkbox   invert, bypass, live;
    juce::TextButton analyseButton { "Analyse" };

    // Painted regions, kept so live readouts can repaint without the canvas.
    juce::Rectangle<int> titleArea, rightColumn, bottomStrip, plateArea, bandArea, artworkArea;

    juce::Image artwork;

    /** Captions for the controls that do not draw their own. Collected during
        layout so paint() has nowhere to disagree with resized() about where
        they go. */
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    int lastAnalyserChoice = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawEQEditor)
};

} // namespace draweq
