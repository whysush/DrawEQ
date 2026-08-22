#pragma once

#include "Controls/BarSlider.h"
#include "Controls/Checkbox.h"
#include "Controls/Console.h"
#include "Controls/SlotStrip.h"
#include "Controls/Toggle.h"
#include "Controls/ToolButton.h"
#include "CurveCanvas.h"

namespace graphite
{

class GraphiteProcessor;

/**
    Slots across the top, canvas and tools on the left, parameters down the
    right.

    The canvas takes every pixel the chrome does not need, because the canvas is
    the instrument and the rest is the settings for it.
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
    void toggleAnalyser();
    int  analyserChoice() const;

    GraphiteProcessor& processor;
    GraphiteLookAndFeel lookAndFeel;

    SlotStrip   slots;
    CurveCanvas canvas;
    Console     console;

    std::array<std::unique_ptr<ToolButton>, 5> toolButtons;
    std::array<std::unique_ptr<Toggle>, 3> modeButtons;
    juce::TextButton analyseButton { "Analyse" };

    BarSlider morph, tilt, smooth, shift, bands, mix, output;
    Checkbox  invert, bypass, live;

    juce::ComboBox analyserBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> analyserAttachment;

    // Regions painted rather than occupied by a child, kept so the status
    // readouts can be repainted without redrawing the canvas.
    juce::Rectangle<int> canvasPanelArea, canvasHeaderArea, toolBarArea, sidebarArea,
                         shapeHeaderArea, setupHeaderArea;

    /** What the analyser returns to when it is switched back on. */
    int lastAnalyserChoice = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphiteEditor)
};

} // namespace graphite
