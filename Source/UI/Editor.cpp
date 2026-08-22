#include "Editor.h"
#include "../Processor.h"

namespace graphite
{

namespace
{
    constexpr int kRowHeight    = 30;
    constexpr int kHeaderHeight = 28;
    constexpr int kCheckHeight  = 24;
    constexpr int kToolCell     = 42;
    constexpr int kModeButton   = 78;

    const Tool kTools[] { Tool::pencil, Tool::line, Tool::smooth, Tool::erase, Tool::node };

    /** Label and the `smooth` percentage it stands for. 1:1 hands the stroke to
        the DSP exactly as drawn, corners included. */
    struct Fidelity { const char* label; float smoothPercent; };
    const Fidelity kFidelity[] { { "1:1", 0.0f }, { "Soft", 15.0f }, { "Smooth", 40.0f } };

    /** A section rule: caption on the left, hairline across the rest. */
    void paintSectionHeader (juce::Graphics& g, juce::Rectangle<int> area,
                             const juce::String& caption, const juce::String& status,
                             juce::Colour statusColour)
    {
        Theme::drawTrackedLabel (g, caption, area.withTrimmedLeft (4),
                                 Theme::Colour::of (Theme::Colour::textHi),
                                 juce::Justification::centredLeft,
                                 Theme::Metrics::smallSize);

        if (status.isNotEmpty())
            Theme::drawTrackedLabel (g, status, area.withTrimmedRight (4),
                                     statusColour, juce::Justification::centredRight);

        g.setColour (Theme::Colour::of (Theme::Colour::hairline));
        g.drawHorizontalLine (area.getBottom() - 1, float (area.getX()), float (area.getRight()));
    }
}

GraphiteEditor::GraphiteEditor (GraphiteProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      slots (p),
      canvas (p),
      console (p),
      morph  (p.apvts, params::id::morph,      "Morph"),
      tilt   (p.apvts, params::id::tilt,       "Tilt"),
      smooth (p.apvts, params::id::smooth,     "Smooth"),
      shift  (p.apvts, params::id::freqShift,  "Shift"),
      bands  (p.apvts, params::id::bandCount,  "Bands"),
      mix    (p.apvts, params::id::mix,        "Mix"),
      output (p.apvts, params::id::outputGain, "Out"),
      invert (p.apvts, params::id::phaseInvert, "Invert"),
      bypass (p.apvts, params::id::bypass,      "Bypass"),
      live   (p.apvts, params::id::liveFit,     "Live fit")
{
    Theme::loadFonts();
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (slots);
    addAndMakeVisible (canvas);
    addAndMakeVisible (console);

    for (auto* b : { &morph, &tilt, &smooth, &shift, &bands, &mix, &output })
        addAndMakeVisible (*b);

    addAndMakeVisible (invert);
    addAndMakeVisible (bypass);
    addAndMakeVisible (live);

    for (std::size_t i = 0; i < toolButtons.size(); ++i)
    {
        auto& b = toolButtons[i];
        b = std::make_unique<ToolButton> (kTools[i]);
        b->onClick = [this, t = kTools[i]] { canvas.setTool (t); };
        addAndMakeVisible (*b);
    }

    canvas.onToolChanged = [this] { refreshToolButtons(); };
    refreshToolButtons();

    const char* modeLabels[] { "Lin", "Min", "Analog" };

    for (int i = 0; i < 3; ++i)
    {
        modeButtons[std::size_t (i)] =
            std::make_unique<Toggle> (p.apvts, params::id::mode, modeLabels[i], i);
        addAndMakeVisible (*modeButtons[std::size_t (i)]);
    }

    // The analyser is one parameter with four values, so this button is an
    // on/off that remembers which of the three "on" values you were using, and
    // the sidebar box picks between them.
    analyseButton.setClickingTogglesState (false);
    analyseButton.setWantsKeyboardFocus (true);
    analyseButton.onClick = [this] { toggleAnalyser(); };
    addAndMakeVisible (analyseButton);

    analyserBox.addItemList ({ "Off", "Pre", "Post", "Both" }, 1);
    addAndMakeVisible (analyserBox);

    shapeBox.setTextWhenNothingSelected ("Load shape");

    for (int i = 0; i < int (shapes::Shape::count); ++i)
        shapeBox.addItem (shapes::name (shapes::Shape (i)), i + 1);

    shapeBox.onChange = [this]
    {
        const int picked = shapeBox.getSelectedId();

        if (picked <= 0)
            return;

        processor.applyShape (shapes::Shape (picked - 1));

        // Back to the prompt, so picking the same shape again re-applies it
        // rather than doing nothing.
        shapeBox.setSelectedId (0, juce::dontSendNotification);
    };

    addAndMakeVisible (shapeBox);

    for (std::size_t i = 0; i < fidelityButtons.size(); ++i)
    {
        auto& b = fidelityButtons[i];
        b.setButtonText (kFidelity[i].label);
        b.setClickingTogglesState (false);
        b.setWantsKeyboardFocus (true);
        b.onClick = [this, target = kFidelity[i].smoothPercent]
        {
            if (auto* smoothParam = processor.apvts.getParameter (params::id::smooth))
            {
                smoothParam->beginChangeGesture();
                smoothParam->setValueNotifyingHost (smoothParam->convertTo0to1 (target));
                smoothParam->endChangeGesture();
            }
        };

        addAndMakeVisible (b);
    }

    analyserAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, params::id::analyzer, analyserBox);

    if (analyserChoice() != 0)
        lastAnalyserChoice = analyserChoice();

   #if JUCE_LINUX
    // The corner grab handle sets a resize mouse cursor, and on Linux that is
    // the same X teardown crash documented in CurveCanvas. Dropping the handle
    // does not drop resizing: the constrainer below still governs, and the host
    // frame is what a user drags in practice.
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

void GraphiteEditor::timerCallback()
{
    analyseButton.setToggleState (analyserChoice() != 0, juce::dontSendNotification);

    const float smoothNow = processor.apvts.getRawParameterValue (params::id::smooth)->load();

    for (std::size_t i = 0; i < fidelityButtons.size(); ++i)
        fidelityButtons[i].setToggleState (std::abs (smoothNow - kFidelity[i].smoothPercent) < 0.5f,
                                           juce::dontSendNotification);

    // Only the strips carrying live numbers; the canvas repaints itself.
    repaint (toolBarArea);
    repaint (canvasHeaderArea);
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

    // --- canvas panel header ----------------------------------------------
    {
        auto area = canvasHeaderArea;

        g.setColour (Theme::Colour::of (Theme::Colour::board));
        g.fillRect (area);

        auto text = area.reduced (12, 0);
        Theme::drawTrackedLabel (g, "Graphite", text.removeFromLeft (150),
                                 Theme::Colour::of (Theme::Colour::textHi),
                                 juce::Justification::centredLeft,
                                 Theme::Metrics::titleSize, 0.12f);

        // The band count the user set, not that plus the two shelves: the
        // sidebar says 12, so this must say 12.
        const int bells = int (processor.apvts.getRawParameterValue (params::id::bandCount)->load());

        const juce::String subtitle = ui.mode == Mode::analog
            ? "eq " + juce::String::fromUTF8 ("\xc2\xb7") + " " + juce::String (bells) + " bands"
            : "eq " + juce::String::fromUTF8 ("\xc2\xb7") + " spectral";

        g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
        g.setColour (Theme::Colour::of (Theme::Colour::textLo));
        g.drawText (subtitle, text.removeFromLeft (140), juce::Justification::centredLeft);
    }

    // --- canvas plate ------------------------------------------------------
    g.setColour (Theme::Colour::of (Theme::Colour::board));
    g.fillRect (canvas.getBounds());

    g.setColour (Theme::Colour::of (Theme::Colour::hairline));
    g.drawRect (canvasPanelArea, 1);

    // --- tool bar ----------------------------------------------------------
    {
        auto area = toolBarArea;

        g.setColour (Theme::Colour::of (Theme::Colour::panel));
        g.fillRect (area);
        g.setColour (Theme::Colour::of (Theme::Colour::hairline));
        g.drawRect (area, 1.0f);

        Theme::drawTrackedLabel (g, "Tool", area.withTrimmedLeft (12).withWidth (54),
                                 Theme::Colour::of (Theme::Colour::textMid),
                                 juce::Justification::centredLeft);

        // MAX ERR: small, permanent, honest. Amber above 3 dB, which is the
        // point where the fitter is no longer telling the truth about the
        // stroke and Spectral mode should be offered (CONTEXT.md 7.5).
        const bool poor = ui.valid && ui.maxErrorDb > 3.0f;
        auto readout = area.removeFromRight (200).reduced (10, 12);

        Theme::drawTrackedLabel (g, "Max err", readout.removeFromLeft (74),
                                 Theme::Colour::of (Theme::Colour::textMid),
                                 juce::Justification::centredLeft);

        g.setColour (Theme::Colour::of (Theme::Colour::recessed));
        g.fillRect (readout);
        g.setColour (Theme::Colour::of (poor ? Theme::Colour::warn : Theme::Colour::hairline));
        g.drawRect (readout.toFloat(), 1.0f);

        g.setFont (Theme::monoFont (Theme::Metrics::smallSize));
        g.setColour (Theme::Colour::of (poor ? Theme::Colour::warn : Theme::Colour::textHi));
        g.drawText (ui.valid ? juce::String (ui.maxErrorDb, 2) + " dB" : "--",
                    readout, juce::Justification::centred);
    }

    // --- sidebar -----------------------------------------------------------
    g.setColour (Theme::Colour::of (Theme::Colour::panel));
    g.fillRect (sidebarArea);
    g.setColour (Theme::Colour::of (Theme::Colour::hairline));
    g.drawRect (sidebarArea, 1.0f);

    paintSectionHeader (g, shapeHeaderArea, "Shape",
                        processor.worker().publishCount() > 0 ? "Online" : "Idle",
                        Theme::Colour::of (processor.worker().publishCount() > 0
                                               ? Theme::Colour::accent : Theme::Colour::textLo));

    paintSectionHeader (g, setupHeaderArea, "Setup", {}, {});
}

void GraphiteEditor::resized()
{
    auto bounds = getLocalBounds();

    // Sidebar spans the full height on the right; everything else stacks on
    // the left.
    sidebarArea = bounds.removeFromRight (Theme::Metrics::sidebarWidth);
    auto sidebar = sidebarArea.reduced (Theme::Metrics::gap);

    slots.setBounds (bounds.removeFromTop (Theme::Metrics::slotBarHeight)
                           .reduced (Theme::Metrics::gap, Theme::Metrics::gap / 2));

    toolBarArea = bounds.removeFromBottom (Theme::Metrics::toolBarHeight)
                        .reduced (Theme::Metrics::gap, Theme::Metrics::gap / 2);

    {
        auto row = toolBarArea.reduced (12, 9);
        row.removeFromLeft (54);   // "TOOL" caption, painted

        for (auto& b : toolButtons)
        {
            b->setBounds (row.removeFromLeft (kToolCell));
            row.removeFromLeft (6);
        }
    }

    auto canvasPanel = bounds.reduced (Theme::Metrics::gap, 0);
    canvasPanelArea = canvasPanel;
    canvasHeaderArea = canvasPanel.removeFromTop (Theme::Metrics::canvasHeader);

    {
        auto row = canvasHeaderArea.reduced (12, 8);
        row.removeFromLeft (300);   // wordmark and subtitle, painted

        analyseButton.setBounds (row.removeFromRight (kModeButton));
        row.removeFromRight (6);

        for (int i = 2; i >= 0; --i)
        {
            modeButtons[std::size_t (i)]->setBounds (row.removeFromRight (kModeButton));
            row.removeFromRight (6);
        }
    }

    canvas.setBounds (canvasPanel.withTrimmedBottom (Theme::Metrics::gap / 2));

    // --- sidebar contents --------------------------------------------------
    shapeHeaderArea = sidebar.removeFromTop (kHeaderHeight);
    sidebar.removeFromTop (Theme::Metrics::gap);

    shapeBox.setBounds (sidebar.removeFromTop (26));
    sidebar.removeFromTop (Theme::Metrics::gap / 2);

    {
        auto row = sidebar.removeFromTop (24);
        const int cell = row.getWidth() / int (fidelityButtons.size());

        for (auto& b : fidelityButtons)
        {
            b.setBounds (row.removeFromLeft (cell).reduced (1, 0));
        }
    }

    sidebar.removeFromTop (Theme::Metrics::gap);

    for (auto* b : { &morph, &tilt, &smooth, &shift, &bands, &mix })
    {
        b->setBounds (sidebar.removeFromTop (kRowHeight));
        sidebar.removeFromTop (3);
    }

    sidebar.removeFromTop (Theme::Metrics::gap);
    setupHeaderArea = sidebar.removeFromTop (kHeaderHeight);
    sidebar.removeFromTop (Theme::Metrics::gap);

    invert.setBounds (sidebar.removeFromTop (kCheckHeight).withTrimmedLeft (4));
    bypass.setBounds (sidebar.removeFromTop (kCheckHeight).withTrimmedLeft (4));
    live.setBounds   (sidebar.removeFromTop (kCheckHeight).withTrimmedLeft (4));

    sidebar.removeFromTop (Theme::Metrics::gap);
    analyserBox.setBounds (sidebar.removeFromTop (26));
    sidebar.removeFromTop (Theme::Metrics::gap);
    output.setBounds (sidebar.removeFromTop (kRowHeight));

    sidebar.removeFromTop (Theme::Metrics::gap * 2);
    console.setBounds (sidebar.removeFromTop (juce::jmin (sidebar.getHeight(), 56)));
}

} // namespace graphite
