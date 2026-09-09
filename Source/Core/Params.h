#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace draweq::params
{

/**
    Parameter IDs are permanent. Adding one is free; renaming or reordering one
    breaks every project a user has saved. If a parameter has
    to be retired, keep its ID and hide it from the editor.
*/
namespace id
{
    inline constexpr const char* bypass      = "bypass";
    inline constexpr const char* mode        = "mode";
    inline constexpr const char* outputGain  = "outputGain";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* tilt        = "tilt";
    inline constexpr const char* smooth      = "smooth";
    inline constexpr const char* freqShift   = "freqShift";
    inline constexpr const char* morph       = "morph";
    inline constexpr const char* morphA      = "morphA";
    inline constexpr const char* morphB      = "morphB";
    inline constexpr const char* bandCount   = "bandCount";
    inline constexpr const char* analyzer    = "analyzer";
    inline constexpr const char* phaseInvert = "phaseInvert";
    inline constexpr const char* liveFit     = "liveFit";
    inline constexpr const char* testTone    = "testTone";
    inline constexpr const char* toneFreq    = "toneFreq";
}

enum class AnalyzerMode { off = 0, pre, post, both };

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace draweq::params
