#include "Editor.h"
#include "../Core/Shapes.h"
#include "../Processor.h"

namespace graphite
{

namespace
{
    constexpr int kToolCell  = 26;
    constexpr int kCaption   = 12;
    constexpr int kRowHeight = Theme::Metrics::rowHeight;

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
      bands  (p.apvts, params::id::bandCount,  "Bands", 34),
      mix    (p.apvts, params::id::mix,        "Mix",   34),
      output (p.apvts, params::id::outputGain, "Out",   34),
      tilt   (p.apvts, params::id::tilt,       "Tilt",  40),
      smooth (p.apvts, params::id::smooth,     "Smth",  40),
      shift  (p.apvts, params::id::freqShift,  "Shift", 40),
      morph  (p.apvts, params::id::morph,      "Mrph",  40),
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

    // --- the band the Node tool has hold of --------------------------------
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

    // Hidden rather than dimmed. Three dead bars take the same room as three
    // live ones, and the space is better spent saying how to get a live one.
    for (auto* k : { &bandFreq, &bandGain, &bandQ })
        k->setVisible (have);

    repaint (bandArea);
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

        auto area = titleArea.reduced (7, 0);

        // A single lamp, amber while the filter is doing something. It is the
        // only ornament on the panel and it is carrying a real fact.
        const bool active = ui.valid && ui.numBands > 0;
        g.setColour (Theme::Colour::of (active ? Theme::Colour::accent : Theme::Colour::recessed));
        g.fillRect (area.getX(), area.getCentreY() - 3, 6, 6);

        area.removeFromLeft (14);
        Theme::drawTrackedLabel (g, "Graphite", area.removeFromLeft (120),
                                 Theme::Colour::of (Theme::Colour::titleText),
                                 juce::Justification::centredLeft,
                                 Theme::Metrics::titleSize, 0.10f);
    }

    // --- the plate ---------------------------------------------------------
    g.setColour (Theme::Colour::of (Theme::Colour::board));
    g.fillRect (plateArea);
    g.setColour (Theme::Colour::of (Theme::Colour::hairline));
    g.drawRect (plateArea, 1);

    // --- bottom strip ------------------------------------------------------
    {
        g.setColour (Theme::Colour::of (Theme::Colour::panel));
        g.fillRect (bottomStrip);

        // MAX ERR sits with the tools because it is a fact about the drawing,
        // not a setting. Amber above 3 dB, where the fitter stops telling the
        // truth about the stroke (CONTEXT.md 7.5).
        const bool poor = ui.valid && ui.maxErrorDb > 3.0f;
        auto readout = bottomStrip.reduced (5, 3).removeFromTop (kRowHeight)
                                  .removeFromRight (84);

        Theme::drawTrackedLabel (g, "Err", readout.removeFromLeft (26),
                                 Theme::Colour::of (Theme::Colour::textMid),
                                 juce::Justification::centredLeft);

        g.setColour (Theme::Colour::of (poor ? Theme::Colour::warn : Theme::Colour::field));
        g.fillRect (readout);
        g.setFont (Theme::monoFont (Theme::Metrics::smallSize));
        g.setColour (Theme::Colour::of (poor ? Theme::Colour::titleText
                                             : Theme::Colour::fieldText));
        g.drawText (ui.valid ? juce::String (ui.maxErrorDb, 2) : juce::String ("--"),
                    readout, juce::Justification::centred);
    }

    // --- captions for the controls that do not draw their own -------------
    for (const auto& c : captions)
        paintCaption (g, c.first, c.second);

    // The band row says which band it is editing, or that there is none.
    {
        Band band;
        const bool have = canvas.selectedBand (band);

        g.setColour (Theme::Colour::of (Theme::Colour::hairline));
        g.fillRect (bandArea.getX() - 4, bandArea.getY() + 2, 1, bandArea.getHeight() - 4);

        if (! have)
        {
            g.setFont (Theme::labelFont (Theme::Metrics::labelSize));
            g.setColour (Theme::Colour::of (Theme::Colour::textLo));
            g.drawText ("node tool " + juce::String::fromUTF8 ("\xc2\xb7")
                            + " click a band to edit it",
                        bandArea, juce::Justification::centredLeft);
        }
        else
        {
            Theme::drawTrackedLabel (g, "Band " + juce::String (band.id + 1),
                                     bandArea.withWidth (0),
                                     Theme::Colour::of (Theme::Colour::textMid),
                                     juce::Justification::centredLeft);
        }
    }
}

void GraphiteEditor::resized()
{
    auto bounds = getLocalBounds();
    captions.clear();

    titleArea = bounds.removeFromTop (Theme::Metrics::titleBarHeight);
    status.setBounds (titleArea.reduced (8, 0).withTrimmedLeft (140));

    bottomStrip = bounds.removeFromBottom (Theme::Metrics::bottomStripHeight);

    {
        auto strip = bottomStrip.reduced (5, 3);
        auto top = strip.removeFromTop (kRowHeight);
        strip.removeFromTop (2);
        auto lower = strip.removeFromTop (kRowHeight);

        // --- upper row: tools, slots, and the error readout ----------------
        {
            auto row = top;
            auto toolRow = row.removeFromLeft (int (toolButtons.size()) * (kToolCell + 2));

            for (auto& b : toolButtons)
            {
                b->setBounds (toolRow.removeFromLeft (kToolCell));
                toolRow.removeFromLeft (2);
            }

            row.removeFromLeft (8);
            row.removeFromRight (86);          // MAX ERR, painted
            slots.setBounds (row.removeFromLeft (juce::jmin (row.getWidth(), 250)));
        }

        // --- lower row: the four macros, then the selected band ------------
        {
            auto row = lower;
            const int macroWidth = 128;

            for (auto* f : { &tilt, &smooth, &shift, &morph })
            {
                f->setBounds (row.removeFromLeft (macroWidth));
                row.removeFromLeft (2);
            }

            row.removeFromLeft (6);
            bandArea = row;

            const int bandWidth = juce::jmax (60, row.getWidth() / 3 - 2);

            for (auto* k : { &bandFreq, &bandGain, &bandQ })
            {
                k->setBounds (row.removeFromLeft (bandWidth));
                row.removeFromLeft (2);
            }
        }
    }

    rightColumn = bounds.removeFromRight (Theme::Metrics::rightColumnWidth);

    {
        auto column = rightColumn.reduced (5, 3);

        analyseButton.setBounds (column.removeFromTop (18));
        column.removeFromTop (2);
        analyserBox.setBounds (column.removeFromTop (18));
        column.removeFromTop (7);

        auto labelled = [&] (juce::Component& c, const juce::String& text)
        {
            captions.emplace_back (column.removeFromTop (kCaption), text);
            c.setBounds (column.removeFromTop (18));
            column.removeFromTop (5);
        };

        labelled (modeBox, "Mode");
        labelled (fidelityBox, "Fidelity");
        labelled (shapeBox, "Shape");

        column.removeFromTop (2);

        for (auto* f : { &bands, &mix, &output })
        {
            f->setBounds (column.removeFromTop (kRowHeight));
            column.removeFromTop (1);
        }

        // The three switches sit at the bottom of the column, so the space the
        // panel has spare collects in one place rather than as a gap after
        // every control.
        auto switches = column.removeFromBottom (18);
        const int cell = switches.getWidth() / 3;
        invert.setBounds (switches.removeFromLeft (cell));
        bypass.setBounds (switches.removeFromLeft (cell));
        live.setBounds   (switches);
    }

    plateArea = bounds.reduced (Theme::Metrics::gap, 3);
    canvas.setBounds (plateArea.reduced (1));
}

} // namespace graphite
