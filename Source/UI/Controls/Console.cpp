#include "Console.h"
#include "../../Processor.h"

namespace draweq
{

Console::Console (DrawEQProcessor& p)
    : processor (p)
{
    startTimerHz (4);   // status, not telemetry: four a second is plenty
}

Console::~Console()
{
    stopTimer();
}

void Console::timerCallback()
{
    repaint();
}

void Console::paint (juce::Graphics& g)
{
    const auto ui = processor.worker().uiSnapshot();

    const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const double latencyMs = 1000.0 * double (processor.getLatencySamples()) / sr;
    const bool poor = ui.valid && ui.maxErrorDb > 3.0f;

    const auto dot = juce::String::fromUTF8 ("\xc2\xb7");

    juce::String text;

    if (processor.worker().commitPending())
    {
        text = "drawing " + dot + " release to fit";
    }
    else if (! ui.valid)
    {
        text = "waiting for first fit";
    }
    else
    {
        // Every figure here is measured. A status line reporting a plausible
        // constant would be worse than no status line.
        if (ui.mode == Mode::analog)
            text = juce::String (juce::jmax (0, ui.numBands - 2)) + " bands " + dot + " ";

        text += "err " + juce::String (ui.maxErrorDb, 2) + " dB " + dot + " "
              + juce::String (latencyMs, latencyMs < 10.0 ? 1 : 0) + " ms " + dot + " "
              + juce::String (processor.audioLoadPercent(), 1) + "%";
    }

    g.setFont (Theme::monoFont (Theme::Metrics::labelSize));
    // The warn colour is chosen to read on the grey chrome, and the title bar
    // is darker than that - so it gets brightened here rather than being left
    // to sit at two thirds contrast where it matters most.
    g.setColour (poor ? Theme::Colour::of (Theme::Colour::warn).brighter (0.55f)
                      : Theme::Colour::of (Theme::Colour::titleText).withAlpha (0.65f));
    g.drawText (text, getLocalBounds(), juce::Justification::centredRight);
}

} // namespace draweq
