#include "Params.h"

namespace graphite::params
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

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::smooth, kVersion }, "Smooth",
        NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 15.0f,
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

    return layout;
}

} // namespace graphite::params
