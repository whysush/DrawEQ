#include "Editor.h"
#include "../Processor.h"

namespace graphite
{

namespace
{
    constexpr int kKnobWidth = 62;
    constexpr int kToolButton = 26;
}

GraphiteEditor::GraphiteEditor (GraphiteProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      canvas (p),
      slots (p),
      morph  (p.apvts, params::id::morph,      "Morph"),
      tilt   (p.apvts, params::id::tilt,       "Tilt"),
      smooth (p.apvts, params::id::smooth,     "Smooth"),
      shift  (p.apvts, params::id::freqShift,  "Shift"),
      bands  (p.apvts, params::id::bandCount,  "Bands"),
      mix    (p.apvts, params::id::mix,        "Mix"),
      output (p.apvts, params::id::outputGain, "Out"),
      invert (p.apvts, params::id::phaseInvert, "INV"),
      bypass (p.apvts, params::id::bypass,      "BYP")
{
    Theme::loadFonts();
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (canvas);
    addAndMakeVisible (slots);

    for (auto* k : { &morph, &tilt, &smooth, &shift, &bands, &mix, &output })
        addAndMakeVisible (*k);

    addAndMakeVisible (invert);
    addAndMakeVisible (bypass);

    const Tool tools[] { Tool::pencil, Tool::line, Tool::smooth, Tool::erase, Tool::node };

    for (std::size_t i = 0; i < toolButtons.size(); ++i)
    {
        auto& b = toolButtons[i];
        b.setButtonText (toolGlyph (tools[i]));
        b.setTooltip (juce::String (toolName (tools[i])) + "  ("
                      + juce::String::charToString (juce::CharacterFunctions::toUpperCase (
                            toolKey (tools[i]))) + ")");
        b.setClickingTogglesState (false);
        b.setWantsKeyboardFocus (true);
        b.onClick = [this, t = tools[i]] { canvas.setTool (t); };
        addAndMakeVisible (b);
    }

    canvas.onToolChanged = [this] { refreshToolButtons(); };
    refreshToolButtons();

    const char* modeLabels[] { "LIN", "MIN", "ANALOG" };

    for (int i = 0; i < 3; ++i)
    {
        modeButtons[std::size_t (i)] =
            std::make_unique<Toggle> (p.apvts, params::id::mode, modeLabels[i], i);
        addAndMakeVisible (*modeButtons[std::size_t (i)]);
    }

    analyzerBox.addItemList ({ "Off", "Pre", "Post", "Both" }, 1);
    analyzerBox.setColour (juce::ComboBox::backgroundColourId,
                           Theme::Colour::of (Theme::Colour::panel));
    analyzerBox.setColour (juce::ComboBox::textColourId, Theme::Colour::of (Theme::Colour::textMid));
    analyzerBox.setColour (juce::ComboBox::outlineColourId, Theme::Colour::of (Theme::Colour::hairline));
    addAndMakeVisible (analyzerBox);

    analyzerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, params::id::analyzer, analyzerBox);

    // The corner grab handle sets a resize mouse cursor, and on Linux that is
    // the same X teardown crash documented in CurveCanvas. Dropping the handle
    // does not drop resizing: the constrainer below still governs, and the host
    // frame is what a user drags in practice.
   #if JUCE_LINUX
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
    canvas.grabKeyboardFocus();
}

GraphiteEditor::~GraphiteEditor()
{
    setLookAndFeel (nullptr);
}

void GraphiteEditor::refreshToolButtons()
{
    const Tool tools[] { Tool::pencil, Tool::line, Tool::smooth, Tool::erase, Tool::node };

    for (std::size_t i = 0; i < toolButtons.size(); ++i)
        toolButtons[i].setToggleState (canvas.tool() == tools[i], juce::dontSendNotification);
}

void GraphiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::Colour::of (Theme::Colour::panel));

    auto bounds = getLocalBounds();
    const auto header = bounds.removeFromTop (Theme::Metrics::headerHeight);
    const auto footer = bounds.removeFromBottom (Theme::Metrics::footerHeight);

    g.setColour (Theme::Colour::of (Theme::Colour::hairline));
    g.drawHorizontalLine (header.getBottom(), 0.0f, float (getWidth()));
    g.drawHorizontalLine (footer.getY(), 0.0f, float (getWidth()));

    g.setFont (Theme::titleFont());
    Theme::drawTrackedLabel (g, "Graphite", header.withTrimmedLeft (14).withWidth (160),
                             Theme::Colour::of (Theme::Colour::textHi),
                             juce::Justification::centredLeft, Theme::Metrics::titleSize, 0.06f);

    Theme::drawTrackedLabel (g, "Spectral", header.withTrimmedRight (250).withWidth (100)
                                                  .withX (getWidth() - 350),
                             Theme::Colour::of (Theme::Colour::textLo),
                             juce::Justification::centredRight);
}

void GraphiteEditor::resized()
{
    auto bounds = getLocalBounds();

    // --- header -----------------------------------------------------------
    auto header = bounds.removeFromTop (Theme::Metrics::headerHeight);
    header.removeFromLeft (180);   // title, painted

    auto toolRow = header.removeFromLeft (int (toolButtons.size()) * (kToolButton + 4))
                         .withSizeKeepingCentre (int (toolButtons.size()) * (kToolButton + 4),
                                                 kToolButton);

    for (auto& b : toolButtons)
    {
        b.setBounds (toolRow.removeFromLeft (kToolButton));
        toolRow.removeFromLeft (4);
    }

    auto modeRow = header.removeFromRight (250).withSizeKeepingCentre (240, kToolButton);

    for (auto& m : modeButtons)
    {
        m->setBounds (modeRow.removeFromLeft (76));
        modeRow.removeFromLeft (6);
    }

    // --- footer -----------------------------------------------------------
    auto footer = bounds.removeFromBottom (Theme::Metrics::footerHeight).reduced (10, 6);

    slots.setBounds (footer.removeFromLeft (200));
    footer.removeFromLeft (14);

    auto rightGroup = footer.removeFromRight (74);
    invert.setBounds (rightGroup.removeFromTop (rightGroup.getHeight() / 2).reduced (0, 3));
    bypass.setBounds (rightGroup.reduced (0, 3));
    footer.removeFromRight (8);

    analyzerBox.setBounds (footer.removeFromRight (72).withSizeKeepingCentre (72, 22));
    footer.removeFromRight (10);

    for (auto* k : { &morph, &tilt, &smooth, &shift, &bands, &mix, &output })
    {
        k->setBounds (footer.removeFromLeft (kKnobWidth));
        footer.removeFromLeft (2);
    }

    // --- canvas takes the rest -------------------------------------------
    canvas.setBounds (bounds);
}

} // namespace graphite
