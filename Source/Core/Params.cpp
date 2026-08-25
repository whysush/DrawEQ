#include "Params.h"

namespace draweq::params
{

using juce::AudioParameterBool;
using juce::AudioParameterChoice;
using juce::AudioParameterFloat;
using juce::AudioParameterInt;
using juce::NormalisableRange;
using juce::ParameterID;

namespace
{
    // Every parameter carries version hint 1. JUCE uses it to keep VST3
    // parameter IDs stable when the layout is extended later.
    constexpr int kVersion = 1;

    juce::String dbSuffix() { return " dB"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { id::bypass, kVersion }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { id::mode, kVersion }, "Mode",
        juce::StringArray { "Spectral Lin", "Spectral Min", "Analog" },
        2));   // Analog: zero latency and an editable band stack is the default

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::outputGain, kVersion }, "Output",
        NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel (dbSuffix())));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::mix, kVersion }, "Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::tilt, kVersion }, "Tilt",
        NormalisableRange<float> { -12.0f, 12.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" dB/dec")));

    // CONTEXT.md 8.2 defaults this to 15 %, which blurs the stroke by about a
    // fifth of an octave before the DSP ever sees it - a cut drawn at -14 dB
    // arrives as -12.8. That made sense when the fit had one frame to work in
    // and needed the help. It does not now: the committed fit handles a sharp
    // stroke, and a drawing tool should give back what was drawn. Smoothing is
    // still one drag away for anyone who wants it.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::smooth, kVersion }, "Smooth",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::freqShift, kVersion }, "Shift",
        NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" st")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::morph, kVersion }, "Morph",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel (" %")));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { id::morphA, kVersion }, "Morph A", 1, 8, 1));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { id::morphB, kVersion }, "Morph B", 1, 8, 2));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { id::bandCount, kVersion }, "Bands", 4, 24, 12));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { id::analyzer, kVersion }, "Analyzer",
        juce::StringArray { "Off", "Pre", "Post", "Both" }, 3));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { id::phaseInvert, kVersion }, "Invert", false));

    // Off by default: the curve is committed when the stroke ends, and the fit
    // that runs then is a far better one than anything affordable at 30 Hz.
    // Turning this on restores continuous re-fitting while dragging.
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { id::liveFit, kVersion }, "Live", false));

    // Off by default, and deliberately so: a plugin that makes noise the
    // moment it is inserted would be a menace. It exists because the
    // standalone has no input, and it is useful in a host for auditioning a
    // curve against something known.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { id::testTone, kVersion }, "Tone",
        juce::StringArray { "Off", "Sine", "Pink", "Sweep" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::toneFreq, kVersion }, "Tone freq",
        NormalisableRange<float> { 20.0f, 20000.0f, 0.0f, 0.25f }, 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel (" Hz")));

    return layout;
}

} // namespace draweq::params
