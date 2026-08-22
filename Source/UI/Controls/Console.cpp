#include "Console.h"
#include "../../Processor.h"

namespace graphite
{

Console::Console (GraphiteProcessor& p)
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

    const bool poor = ui.maxErrorDb > 3.0f;

    const juce::String modeName = ui.mode == Mode::analog          ? "analog"
                                : ui.mode == Mode::spectralLinear  ? "spectral lin"
                                                                   : "spectral min";

    juce::StringArray lines;

    if (processor.worker().commitPending())
    {
        lines.add ("> drawing " + juce::String::fromUTF8 ("\xe2\x80\x94")
                   + " release to fit");
    }
    else if (! ui.valid)
    {
        lines.add ("> waiting for first fit");
    }
    else if (ui.mode == Mode::analog)
    {
        // numBands counts the two shelves; the user set the bell count, and
        // the header says that, so this must agree with the header.
        const int bells = juce::jmax (0, ui.numBands - 2);

        lines.add ("> fit " + juce::String (poor ? "poor" : "ok")
                   + " " + juce::String::fromUTF8 ("\xe2\x80\x94") + " "
                   + juce::String (bells) + " bands, err "
                   + juce::String (ui.maxErrorDb, 2) + " dB");
    }
    else
    {
        lines.add ("> spectral " + juce::String::fromUTF8 ("\xe2\x80\x94") + " err "
                   + juce::String (ui.maxErrorDb, 2) + " dB");
    }

    lines.add ("> latency " + juce::String (latencyMs, latencyMs < 10.0 ? 1 : 0)
               + " ms " + juce::String::fromUTF8 ("\xc2\xb7") + " load "
               + juce::String (processor.audioLoadPercent(), 1) + "%");

    lines.add ("> " + modeName + " " + juce::String::fromUTF8 ("\xc2\xb7") + " "
               + juce::String (sr / 1000.0, 1) + " kHz");

    g.setFont (Theme::monoFont (Theme::Metrics::labelSize));

    auto area = getLocalBounds().reduced (2, 0);
    const int lineHeight = 16;

    for (int i = 0; i < lines.size(); ++i)
    {
        const bool pending = processor.worker().commitPending() && i == 0;
        const bool warn = ! pending && poor && i == 0 && ui.valid && ui.mode == Mode::analog;
        g.setColour (Theme::Colour::of (pending ? Theme::Colour::accent
                                       : warn    ? Theme::Colour::warn
                                       : i == 0  ? Theme::Colour::textMid
                                                 : Theme::Colour::textLo));
        g.drawText (lines[i], area.removeFromTop (lineHeight), juce::Justification::centredLeft);
    }
}

} // namespace graphite
