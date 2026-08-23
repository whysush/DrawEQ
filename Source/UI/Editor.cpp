#include "Editor.h"
#include "../Core/Shapes.h"
#include "../Processor.h"

namespace graphite
{

namespace
{
    constexpr int kToolCell  = 34;
    constexpr int kCaption   = 15;
    constexpr int kRowHeight = Theme::Metrics::rowHeight;
    constexpr int kBoxHeight = 24;

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
      bands  (p.apvts, params::id::bandCount,  "Bands", 44),
      mix    (p.apvts, params::id::mix,        "Mix",   44),
      output (p.apvts, params::id::outputGain, "Out",   44),
      tilt   (p.apvts, params::id::tilt,       "Tilt",  50),
      smooth (p.apvts, params::id::smooth,     "Smooth", 50),
      shift  (p.apvts, params::id::freqShift,  "Shift", 50),
      morph  (p.apvts, params::id::morph,      "Morph", 50),
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

        auto area = titleArea.reduced (9, 0);

        // A single lamp, amber while the filter is doing something. It is the
        // only ornament on the panel and it is carrying a real fact.
        const bool active = ui.valid && ui.numBands > 0;
        const auto lamp = juce::Rectangle<int> (8, 8).withCentre ({ area.getX() + 4,
                                                                    area.getCentreY() });
        g.setColour (Theme::Colour::of (active ? Theme::Colour::accent : Theme::Colour::recessed));
        g.fillRect (lamp);
        Theme::drawPixelBevel (g, lamp, true);

        area.removeFromLeft (18);
        Theme::drawTrackedLabel (g, "Graphite", area.removeFromLeft (128),
                                 Theme::Colour::of (Theme::Colour::titleText),
                                 juce::Justification::centredLeft,
                                 Theme::Metrics::titleSize, 0.10f);

        Theme::drawTrackedLabel (g, "Drawable EQ", area.removeFromLeft (110),
                                 Theme::Colour::of (Theme::Colour::textLo),
                                 juce::Justification::centredLeft);
    }

    // --- the plate, recessed into the face --------------------------------
    g.setColour (Theme::Colour::of (Theme::Colour::board));
    g.fillRect (plateArea);
    Theme::drawPixelBevel (g, plateArea, false);

    // --- bottom strip ------------------------------------------------------
    {
        g.setColour (Theme::Colour::of (Theme::Colour::panel));
        g.fillRect (bottomStrip);
        Theme::drawPixelBevel (g, bottomStrip, true);

        // The error readout sits with the tools because it is a fact about the
        // drawing, not a setting. Amber above 3 dB, where the fitter stops
        // telling the truth about the stroke (CONTEXT.md 7.5).
        const bool poor = ui.valid && ui.maxErrorDb > 3.0f;
        auto readout = bottomStrip.reduced (6, 6).removeFromRight (112);

        Theme::drawTrackedLabel (g, "Err", readout.removeFromLeft (34),
                                 Theme::Colour::of (Theme::Colour::textMid),
                                 juce::Justification::centredLeft);

        g.setColour (Theme::Colour::of (poor ? Theme::Colour::warn : Theme::Colour::field));
        g.fillRect (readout);
        Theme::drawPixelBevel (g, readout, false);

        g.setFont (Theme::monoFont (Theme::Metrics::bodySize));
        g.setColour (Theme::Colour::of (poor ? Theme::Colour::titleText
                                             : Theme::Colour::fieldText));
        g.drawText (ui.valid ? juce::String (ui.maxErrorDb, 2) : juce::String ("--"),
                    readout, juce::Justification::centred);
    }

    // --- right column ------------------------------------------------------
    {
        g.setColour (Theme::Colour::of (Theme::Colour::panel));
        g.fillRect (rightColumn);
        Theme::drawPixelBevel (g, rightColumn, true);
    }

    for (const auto& c : captions)
        paintCaption (g, c.first, c.second);

    // --- the band row says what it is editing -----------------------------
    {
        Band band;
        const bool have = canvas.selectedBand (band);

        g.setColour (Theme::Colour::of (Theme::Colour::bevelDark));
        g.fillRect (bandArea.getX() - 7, bandArea.getY(), 1, bandArea.getHeight());
        g.setColour (Theme::Colour::of (Theme::Colour::bevelLight));
        g.fillRect (bandArea.getX() - 6, bandArea.getY(), 1, bandArea.getHeight());

        if (! have)
        {
            g.setFont (Theme::labelFont (Theme::Metrics::labelSize));
            g.setColour (Theme::Colour::of (Theme::Colour::textLo));
            g.drawText ("node tool " + juce::String::fromUTF8 ("\xc2\xb7")
                            + " click a band to edit it",
                        bandArea, juce::Justification::centredLeft);
        }
    }
}

void GraphiteEditor::resized()
{
    auto bounds = getLocalBounds();
    captions.clear();

    titleArea = bounds.removeFromTop (Theme::Metrics::titleBarHeight);
    status.setBounds (titleArea.reduced (10, 0).withTrimmedLeft (260));

    // --- one row along the bottom: tools, slots, the selected band, error ---
    bottomStrip = bounds.removeFromBottom (Theme::Metrics::bottomStripHeight);

    {
        auto row = bottomStrip.reduced (6, 6);

        auto toolRow = row.removeFromLeft (int (toolButtons.size()) * (kToolCell + 2));

        for (auto& b : toolButtons)
        {
            b->setBounds (toolRow.removeFromLeft (kToolCell));
            toolRow.removeFromLeft (2);
        }

        row.removeFromLeft (14);
        row.removeFromRight (112);            // ERR, painted
        row.removeFromRight (10);

        // The band section is sized first and the slot strip takes what is
        // left, so widening the window widens the slots rather than opening a
        // gap in the middle of the row.
        bandArea = row.removeFromRight (330);
        row.removeFromRight (12);
        slots.setBounds (row);

        auto band = bandArea;
        const int bandWidth = band.getWidth() / 3 - 2;

        for (auto* k : { &bandFreq, &bandGain, &bandQ })
        {
            k->setBounds (band.removeFromLeft (bandWidth));
            band.removeFromLeft (2);
        }
    }

    // --- everything else down the right ------------------------------------
    rightColumn = bounds.removeFromRight (Theme::Metrics::rightColumnWidth);

    {
        auto column = rightColumn.reduced (8, 6);

        analyseButton.setBounds (column.removeFromTop (kBoxHeight));
        column.removeFromTop (3);
        analyserBox.setBounds (column.removeFromTop (kBoxHeight));
        column.removeFromTop (9);

        auto labelled = [&] (juce::Component& c, const juce::String& text)
        {
            captions.emplace_back (column.removeFromTop (kCaption), text);
            c.setBounds (column.removeFromTop (kBoxHeight));
            column.removeFromTop (6);
        };

        labelled (modeBox, "Mode");
        labelled (fidelityBox, "Fidelity");
        labelled (shapeBox, "Shape");

        // The switches are pinned to the bottom first, so the faders divide
        // what is actually left rather than leaving a void above them.
        auto switches = column.removeFromBottom (kBoxHeight);
        const int cell = switches.getWidth() / 3;
        invert.setBounds (switches.removeFromLeft (cell));
        bypass.setBounds (switches.removeFromLeft (cell));
        live.setBounds   (switches);

        column.removeFromBottom (8);

        Fader* faders[] { &bands, &mix, &output, &tilt, &smooth, &shift, &morph };
        const int each = column.getHeight() / int (std::size (faders));

        for (auto* f : faders)
            f->setBounds (column.removeFromTop (each));
    }

    plateArea = bounds.reduced (Theme::Metrics::gap, 4);
    canvas.setBounds (plateArea.reduced (1));
}

} // namespace graphite
