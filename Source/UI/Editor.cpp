#include "Editor.h"
#include "../Core/Shapes.h"
#include "../Processor.h"

namespace graphite
{

namespace
{
    constexpr int kToolCell    = 34;
    constexpr int kCaption     = 14;
    constexpr int kFieldHeight = 32;
    constexpr int kRowHeight   = 20;

    const Tool kTools[] { Tool::pencil, Tool::line, Tool::smooth, Tool::erase, Tool::node };

    struct Fidelity { const char* label; float smoothPercent; };
    const Fidelity kFidelity[] { { "1:1", 0.0f }, { "Soft", 15.0f }, { "Smooth", 40.0f } };

    /** Caption above a control, in the chrome's own dark text. */
    void paintCaption (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
    {
        Theme::drawTrackedLabel (g, text, area.withHeight (kCaption),
                                 Theme::Colour::of (Theme::Colour::textMid),
                                 juce::Justification::centredLeft);
    }
}

GraphiteEditor::GraphiteEditor (GraphiteProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      canvas (p),
      slots (p),
      status (p),
      bands  (p.apvts, params::id::bandCount,  "Bands"),
      mix    (p.apvts, params::id::mix,        "Mix"),
      output (p.apvts, params::id::outputGain, "Gain"),
      tilt   (p.apvts, params::id::tilt,       "Tilt"),
      smooth (p.apvts, params::id::smooth,     "Smooth"),
      shift  (p.apvts, params::id::freqShift,  "Shift"),
      morph  (p.apvts, params::id::morph,      "Morph"),
      invert (p.apvts, params::id::phaseInvert, "Inv"),
      bypass (p.apvts, params::id::bypass,      "Byp"),
      live   (p.apvts, params::id::liveFit,     "Live")
{
    Theme::loadFonts();
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (canvas);
    addAndMakeVisible (slots);
    addAndMakeVisible (status);

    for (auto* f : { &bands, &mix, &output, &tilt, &smooth, &shift, &morph })
        addAndMakeVisible (*f);

    for (auto* c : { &invert, &bypass, &live })
        addAndMakeVisible (*c);

    // --- left column: the band the Node tool has hold of -------------------
    bandFreq.setSkewForFrequency();

    for (auto* k : { &bandFreq, &bandGain, &bandQ })
    {
        k->onValueChanged = [this] (double) { pushBandEdit(); };
        addAndMakeVisible (*k);
    }

    canvas.onBandSelectionChanged = [this] { refreshBandColumn(); };
    refreshBandColumn();

    // --- tools -------------------------------------------------------------
    for (std::size_t i = 0; i < toolButtons.size(); ++i)
    {
        auto& b = toolButtons[i];
        b = std::make_unique<ToolButton> (kTools[i]);
        b->onClick = [this, t = kTools[i]] { canvas.setTool (t); };
        addAndMakeVisible (*b);
    }

    canvas.onToolChanged = [this] { refreshToolButtons(); };
    refreshToolButtons();

    // --- right column ------------------------------------------------------
    modeBox.addItemList ({ "Linear", "Minimum", "Analog" }, 1);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, params::id::mode, modeBox);

    for (std::size_t i = 0; i < std::size (kFidelity); ++i)
        fidelityBox.addItem (kFidelity[i].label, int (i) + 1);

    fidelityBox.onChange = [this]
    {
        const int picked = fidelityBox.getSelectedId();

        if (picked <= 0)
            return;

        // Sets the real parameter rather than shadowing it, so there is no
        // second copy of the state to drift out of sync.
        if (auto* smoothParam = processor.apvts.getParameter (params::id::smooth))
        {
            const float target = kFidelity[std::size_t (picked - 1)].smoothPercent;
            smoothParam->beginChangeGesture();
            smoothParam->setValueNotifyingHost (smoothParam->convertTo0to1 (target));
            smoothParam->endChangeGesture();
        }
    };

    addAndMakeVisible (fidelityBox);

    shapeBox.setTextWhenNothingSelected ("Load");

    for (int i = 0; i < int (shapes::Shape::count); ++i)
        shapeBox.addItem (shapes::name (shapes::Shape (i)), i + 1);

    shapeBox.onChange = [this]
    {
        const int picked = shapeBox.getSelectedId();

        if (picked <= 0)
            return;

        processor.applyShape (shapes::Shape (picked - 1));

        // Back to the prompt, so picking the same shape twice re-applies it
        // rather than doing nothing.
        shapeBox.setSelectedId (0, juce::dontSendNotification);
    };

    addAndMakeVisible (shapeBox);

    analyserBox.addItemList ({ "Off", "Pre", "Post", "Both" }, 1);
    addAndMakeVisible (analyserBox);
    analyserAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, params::id::analyzer, analyserBox);

    // One parameter with four values, so this button is an on/off that
    // remembers which of the three "on" values was last in use.
    analyseButton.setClickingTogglesState (false);
    analyseButton.setWantsKeyboardFocus (true);
    analyseButton.onClick = [this] { toggleAnalyser(); };
    addAndMakeVisible (analyseButton);

    if (analyserChoice() != 0)
        lastAnalyserChoice = analyserChoice();

   #if JUCE_LINUX
    // The corner grab handle sets a resize mouse cursor, and on Linux that is
    // the X teardown crash documented in CurveCanvas. Resizing still works -
    // the constrainer governs it and the host frame drives it.
    setResizable (true, false);
   #else
    setResizable (true, true);
   #endif

    if (auto* limits = getConstrainer())
    {
        limits->setFixedAspectRatio (double (Theme::Metrics::defaultWidth)
                                   / double (Theme::Metrics::defaultHeight));
        limits->setSizeLimits (int (Theme::Metrics::defaultWidth * 0.75),
                               int (Theme::Metrics::defaultHeight * 0.75),
                               Theme::Metrics::defaultWidth * 2,
                               Theme::Metrics::defaultHeight * 2);
    }

    setSize (Theme::Metrics::defaultWidth, Theme::Metrics::defaultHeight);
    startTimerHz (8);
    canvas.grabKeyboardFocus();
}

GraphiteEditor::~GraphiteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int GraphiteEditor::analyserChoice() const
{
    if (auto* v = processor.apvts.getRawParameterValue (params::id::analyzer))
        return int (v->load());

    return 0;
}

void GraphiteEditor::toggleAnalyser()
{
    auto* p = processor.apvts.getParameter (params::id::analyzer);

    if (p == nullptr)
        return;

    const int current = analyserChoice();
    const int wanted  = current != 0 ? 0 : juce::jmax (1, lastAnalyserChoice);

    if (current != 0)
        lastAnalyserChoice = current;

    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (float (wanted)));
    p->endChangeGesture();
}

void GraphiteEditor::refreshBandColumn()
{
    Band band;
    const bool have = canvas.selectedBand (band);

    // Writing to the knobs fires their callbacks, which would immediately push
    // the values straight back into the curve and re-fit. The guard is what
    // keeps selecting a band from counting as editing it.
    suppressBandCallback = true;

    if (have)
    {
        bandFreq.setValue (double (band.freqHz));
        bandGain.setValue (double (band.gainDb));
        bandQ.setValue (double (band.q));
    }

    suppressBandCallback = false;

    for (auto* k : { &bandFreq, &bandGain, &bandQ })
        k->setEnabledLook (have);

    repaint (leftColumn);
}

void GraphiteEditor::pushBandEdit()
{
    if (suppressBandCallback)
        return;

    canvas.updateSelectedBand (float (bandFreq.getValue()),
                               float (bandGain.getValue()),
                               float (bandQ.getValue()));
}

void GraphiteEditor::timerCallback()
{
    analyseButton.setToggleState (analyserChoice() != 0, juce::dontSendNotification);

    // Keep the fidelity box showing whichever preset the smooth value matches,
    // and nothing when it sits between them.
    if (auto* v = processor.apvts.getRawParameterValue (params::id::smooth))
    {
        const float now = v->load();
        int matched = 0;

        for (std::size_t i = 0; i < std::size (kFidelity); ++i)
            if (std::abs (now - kFidelity[i].smoothPercent) < 0.5f)
                matched = int (i) + 1;

        if (fidelityBox.getSelectedId() != matched)
            fidelityBox.setSelectedId (matched, juce::dontSendNotification);
    }

    // The band under edit can move while it is being dragged on the canvas.
    if (! bandFreq.isMouseButtonDownAnywhere())
        refreshBandColumn();

    repaint (bottomStrip);
}

void GraphiteEditor::refreshToolButtons()
{
    for (std::size_t i = 0; i < toolButtons.size(); ++i)
        toolButtons[i]->setToggleState (canvas.tool() == kTools[i], juce::dontSendNotification);
}

void GraphiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::Colour::of (Theme::Colour::background));

    const auto ui = processor.worker().uiSnapshot();

    // --- title strip -------------------------------------------------------
    {
        g.setColour (Theme::Colour::of (Theme::Colour::titleBar));
        g.fillRect (titleArea);

        auto area = titleArea.reduced (10, 0);

        // A single lamp, amber when the filter is doing something and dark when
        // it is flat. It is the only ornament on the panel and it is carrying a
        // real fact.
        const bool active = ui.valid && ui.maxErrorDb >= 0.0f && ui.numBands > 0;
        const auto lamp = juce::Rectangle<float> (11.0f, 11.0f)
                              .withCentre ({ float (area.getX()) + 6.0f,
                                             float (area.getCentreY()) });
        g.setColour (Theme::Colour::of (active ? Theme::Colour::accent : Theme::Colour::recessed));
        g.fillEllipse (lamp);

        area.removeFromLeft (24);
        Theme::drawTrackedLabel (g, "Graphite", area.removeFromLeft (150),
                                 Theme::Colour::of (Theme::Colour::titleText),
                                 juce::Justification::centredLeft,
                                 Theme::Metrics::titleSize, 0.02f);
    }

    // --- the plate ---------------------------------------------------------
    g.setColour (Theme::Colour::of (Theme::Colour::board));
    g.fillRect (plateArea);
    g.setColour (Theme::Colour::of (Theme::Colour::recessed));
    g.drawRect (plateArea, 1);

    // --- column captions ---------------------------------------------------
    {
        Band band;
        const bool have = canvas.selectedBand (band);

        auto caption = leftColumn.withHeight (kCaption).reduced (6, 0);
        Theme::drawTrackedLabel (g, have ? "Band " + juce::String (band.id + 1) : "No band",
                                 caption,
                                 Theme::Colour::of (have ? Theme::Colour::textHi
                                                         : Theme::Colour::textLo),
                                 juce::Justification::centred);

        if (! have)
        {
            // Under the dials, not over them: the dials are still the subject
            // even when there is nothing selected to put in them.
            g.setFont (Theme::labelFont (Theme::Metrics::labelSize));
            g.setColour (Theme::Colour::of (Theme::Colour::textLo));
            g.drawFittedText ("Node tool,\nthen click\na band",
                              leftColumn.withTrimmedTop (leftColumn.getHeight() - 56)
                                        .reduced (6, 4),
                              juce::Justification::centredTop, 3);
        }
    }

    // --- bottom strip ------------------------------------------------------
    {
        auto area = bottomStrip;

        g.setColour (Theme::Colour::of (Theme::Colour::panel));
        g.fillRect (area);

        auto left = area.reduced (8, 0);
        paintCaption (g, left.withY (area.getY() + 4).withWidth (40), "Tool");

        // MAX ERR sits with the tools because it is a fact about the drawing,
        // not a setting. Amber above 3 dB, where the fitter stops telling the
        // truth about the stroke (CONTEXT.md 7.5).
        const bool poor = ui.valid && ui.maxErrorDb > 3.0f;
        auto readout = area.removeFromRight (150).reduced (8, 14);

        paintCaption (g, readout.removeFromLeft (56).withY (readout.getY() + 1), "Max err");

        g.setColour (Theme::Colour::of (poor ? Theme::Colour::warn : Theme::Colour::field));
        g.fillRoundedRectangle (readout.toFloat(), 2.0f);
        g.setFont (Theme::monoFont (Theme::Metrics::smallSize));
        g.setColour (Theme::Colour::of (poor ? Theme::Colour::titleText
                                             : Theme::Colour::fieldText));
        g.drawText (ui.valid ? juce::String (ui.maxErrorDb, 2) : juce::String ("--"),
                    readout, juce::Justification::centred);
    }

    // --- captions for the controls that do not draw their own -------------
    for (const auto& c : captions)
        paintCaption (g, c.first, c.second);
}

void GraphiteEditor::resized()
{
    auto bounds = getLocalBounds();

    titleArea = bounds.removeFromTop (Theme::Metrics::titleBarHeight);
    status.setBounds (titleArea.reduced (12, 0).withTrimmedLeft (200));

    bottomStrip = bounds.removeFromBottom (Theme::Metrics::bottomStripHeight);

    {
        auto row = bottomStrip.reduced (8, 0);
        row.removeFromRight (150);            // MAX ERR, painted

        auto toolRow = row.removeFromLeft (44 + int (toolButtons.size()) * (kToolCell + 4));
        toolRow.removeFromLeft (44);          // "Tool" caption, painted
        toolRow = toolRow.withSizeKeepingCentre (toolRow.getWidth(), kToolCell);

        for (auto& b : toolButtons)
        {
            b->setBounds (toolRow.removeFromLeft (kToolCell));
            toolRow.removeFromLeft (4);
        }

        row.removeFromLeft (10);

        // The four macros live down here with the tools rather than in the
        // right column: they act on the drawing, and the drawing is what the
        // bottom of the panel is about.
        auto macros = row.removeFromRight (4 * 74).withSizeKeepingCentre (4 * 74, kFieldHeight);

        for (auto* f : { &tilt, &smooth, &shift, &morph })
        {
            f->setBounds (macros.removeFromLeft (70));
            macros.removeFromLeft (4);
        }

        row.removeFromRight (10);
        slots.setBounds (row.withSizeKeepingCentre (row.getWidth(), 26));
    }

    leftColumn  = bounds.removeFromLeft (Theme::Metrics::leftColumnWidth);
    rightColumn = bounds.removeFromRight (Theme::Metrics::rightColumnWidth);

    // --- left column -------------------------------------------------------
    {
        auto column = leftColumn.reduced (6, 4);
        column.removeFromTop (kCaption + 2);   // "Band n", painted

        const int knobHeight = juce::jmin (78, column.getHeight() / 3 - 4);

        for (auto* k : { &bandFreq, &bandGain, &bandQ })
        {
            k->setBounds (column.removeFromTop (knobHeight));
            column.removeFromTop (4);
        }
    }

    // --- right column ------------------------------------------------------
    {
        auto column = rightColumn.reduced (6, 4);

        captions.clear();

        analyseButton.setBounds (column.removeFromTop (22));
        column.removeFromTop (4);
        analyserBox.setBounds (column.removeFromTop (kRowHeight));
        column.removeFromTop (10);

        auto labelled = [&] (juce::Component& c, const juce::String& text)
        {
            captions.emplace_back (column.removeFromTop (kCaption), text);
            c.setBounds (column.removeFromTop (kRowHeight));
            column.removeFromTop (6);
        };

        labelled (modeBox, "Mode");
        labelled (fidelityBox, "Fidelity");
        labelled (shapeBox, "Shape");

        for (auto* f : { &bands, &mix, &output })
        {
            f->setBounds (column.removeFromTop (kFieldHeight));
            column.removeFromTop (4);
        }

        column.removeFromTop (6);

        auto switches = column.removeFromTop (22);
        const int cell = switches.getWidth() / 3;
        invert.setBounds (switches.removeFromLeft (cell).reduced (1, 0));
        bypass.setBounds (switches.removeFromLeft (cell).reduced (1, 0));
        live.setBounds   (switches.reduced (1, 0));
    }

    plateArea = bounds.reduced (Theme::Metrics::gap, 4);
    canvas.setBounds (plateArea.reduced (1));
}

} // namespace graphite
