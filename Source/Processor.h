#pragma once

#include "Core/CurveModel.h"
#include "Core/Params.h"
#include "Core/PresetBank.h"
#include "Core/Shapes.h"
#include "Core/Shapes.h"
#include "DSP/Analyzer.h"
#include "DSP/BiquadCascade.h"
#include "DSP/ConvolutionEngine.h"
#include "DSP/CurveWorker.h"
#include "DSP/Smoothers.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace graphite
{

/**
    Everything on the audio thread lives here, and everything here obeys
    CONTEXT.md 5: no allocation, no locks, no logging, no destruction of
    anything non-trivial.

    The processor never builds a filter. It receives one, crossfades to it, and
    hands the old one back.
*/
class GraphiteProcessor final : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    GraphiteProcessor();
    ~GraphiteProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;   // the double-precision overload stays default
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    /** Hands the host our own bypass rather than letting the wrapper synthesise
        one.

        Without this a VST3 host sees two bypass controls - ours and JUCE's -
        and its own bypass button drives the synthetic one, which cuts hard
        instead of going through the 20 ms ramp and the latency-compensated dry
        path. One control, and the host's button does the right thing. */
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRAPHITE"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- editor-facing (message thread only) -------------------------------

    CurveModel&  curve()    noexcept { return model; }
    CurveWorker& worker()   noexcept { return curveWorker; }
    Analyzer&    analyzer() noexcept { return spectrum; }
    PresetBank&  bank()     noexcept { return presets; }

    /** Audio callback cost as a percentage of the time available to it.
        Measured, exponentially smoothed, and read by the status console. */
    float audioLoadPercent() const noexcept
    {
        return audioLoad.load (std::memory_order_relaxed) * 100.0f;
    }

    juce::AudioProcessorValueTreeState apvts;

    /** Slot handling. The canvas always edits the slot named by `morphA`, so
        slot A is literally "what you drew" and morph moves from it toward slot
        B. That is what keeps morph continuous at zero. */
    /** Replaces the drawn curve with a starting shape. One undo entry, and the
        committed-quality fit, because a shape is a finished curve. */
    void applyShape (shapes::Shape);

    void storeCurrentIntoSlot (int slot);
    void recallSlot (int slot);
    void clearSlot (int slot);

private:
    void timerCallback() override;
    void pushMacrosToWorker();
    void adoptState (FilterState* incoming) noexcept;
    void renderAnalog (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
    void renderSpectral (juce::AudioBuffer<float>& buffer, int numSamples,
                         const float* irA, const float* irB,
                         float* const* destA, float* const* destB) noexcept;

    static constexpr int kMaxChannels = 2;
    static constexpr float kCrossfadeMs = 20.0f;

    CurveModel  model;
    PresetBank  presets;
    CurveWorker curveWorker;
    Analyzer    spectrum;

    BiquadCascade cascade;
    std::array<ConvolutionEngine, kMaxChannels> engines;

    // Audio-thread-owned pointers into the worker's pool.
    FilterState* active = nullptr;
    FilterState* fading = nullptr;
    int   fadeSamplesLeft = 0;
    int   fadeSamplesTotal = 0;
    Mode  activeMode = Mode::analog;
    Mode  fadingMode = Mode::analog;

    // Scratch, sized in prepareToPlay and never resized afterwards.
    juce::AudioBuffer<float> dryBuffer, pathNew, pathOld;
    std::array<std::vector<float>, kMaxChannels> dryDelay;
    int dryDelayWrite = 0, dryDelayMask = 0;

    OnePole trimSmoother, gainSmoother, mixSmoother;

    // Bypass ramps linearly rather than exponentially. An exponential approach
    // is fine for a gain, but a bypass has to actually arrive: at 20 ms and a
    // one-pole it was still letting through half a percent of the processed
    // signal, which is not what "bypassed" means.
    float bypassRamp = 0.0f;
    float bypassStep = 1.0f;

    double sr = 48000.0;
    int    maxBlock = 512;

    // Written on the message thread, read on the audio thread. Plain ints would
    // be a data race - benign in practice on every architecture we target, and
    // still undefined behaviour that a sanitiser will rightly complain about.
    std::atomic<int>  reportedLatency { 0 };
    std::atomic<bool> prepared { false };
    std::atomic<float> audioLoad { 0.0f };

    // Cached parameter pointers: reading these is a relaxed atomic load, which
    // is the only kind of parameter access allowed on the audio thread.
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pTilt = nullptr;
    std::atomic<float>* pSmooth = nullptr;
    std::atomic<float>* pShift = nullptr;
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pMorphA = nullptr;
    std::atomic<float>* pMorphB = nullptr;
    std::atomic<float>* pBands = nullptr;
    std::atomic<float>* pAnalyzer = nullptr;
    std::atomic<float>* pInvert = nullptr;
    std::atomic<float>* pLive = nullptr;

    juce::AudioProcessorParameter* bypassParameter = nullptr;

    int lastMorphA = 1, lastMorphB = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphiteProcessor)
};

} // namespace graphite
