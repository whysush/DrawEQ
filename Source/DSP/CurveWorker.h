#pragma once

#include "../Core/CurveModel.h"
#include "BandMatcher.h"
#include "ConvolutionEngine.h"
#include "CurveFitter.h"
#include "FilterState.h"
#include "IRBuilder.h"
#include "StateRing.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace graphite
{

/**
    The middle thread. Reads the curve, does all the expensive work, publishes
    a FilterState, and never touches the audio thread's rules.

    Cadence is the point. The loop wakes on a fixed 1/30 s tick rather than on
    every edit, so a fast drag that generates two hundred mouse moves produces
    at most thirty fits - the intermediate strokes are dropped, not queued,
    because a fit of a curve the user has already drawn past is worthless
    (CONTEXT.md 5).
*/
class CurveWorker
{
public:
    /** Everything the editor wants to draw. Copied under a mutex on the message
        thread; deliberately not part of FilterState, so that a FilterState's
        lifetime is decided by the audio thread alone. */
    struct UiSnapshot
    {
        CurveArray targetDb {};      // what the DSP was asked for (post-macros)
        CurveArray achievedDb {};    // what it actually does
        std::array<Band, kMaxBands> bands {};
        int   numBands   = 0;
        float trimDb     = 0.0f;
        float maxErrorDb = 0.0f;
        float rmsErrorDb = 0.0f;
        Mode  mode       = Mode::analog;
        std::uint64_t sourceVersion = 0;
        bool  valid = false;
    };

    CurveWorker();
    ~CurveWorker();

    void setSource (CurveModel* model) noexcept { source = model; }

    /** Message thread, audio stopped. Sizes every pool entry for this
        configuration; nothing after this allocates on the publish path. */
    void prepare (double sampleRate, int maxBlockSize);

    void start();
    void stop();

    /** Macro values, pushed from the processor. Cheap enough to call per block:
        five relaxed stores and no allocation. */
    void setMacros (float tiltDbPerDecade, float smoothPercent, float freqShiftSemitones,
                    int bandCount, Mode mode) noexcept;

    /** The far end of the morph. Set on the message thread whenever the target
        slot changes; the amount itself is automatable and moves independently. */
    void setMorphTarget (const CurveArray& target);
    void setMorphAmount (float zeroToOne) noexcept
    {
        morphAmount.store (zeroToOne, std::memory_order_relaxed);
    }

    /** Forces the next fit to re-seed from scratch: preset load, paste, band
        count change - anything where the previous solution is not a hint but a
        distraction. */
    void requestColdFit() noexcept { coldRequested.store (true, std::memory_order_release); }

    StateRing<FilterState>& ring() noexcept { return states; }

    UiSnapshot uiSnapshot() const;

    /** Latency of the most recently published state, so the processor can call
        setLatencySamples off the message thread. */
    int publishedLatency() const noexcept { return latency.load (std::memory_order_acquire); }
    std::uint64_t publishCount() const noexcept { return published.load (std::memory_order_acquire); }

    int irLength() const noexcept      { return irBuilder.irLength(); }
    int partitionSize() const noexcept { return partition; }

    /** Runs one build cycle synchronously. Only for tests - the plugin always
        goes through the thread. */
    void buildOnceForTesting() { buildIfNeeded(); }

private:
    void run();
    void buildIfNeeded();
    void buildSpectral (FilterState& state, const CurveArray& target, bool minimumPhase);
    void buildAnalog (FilterState& state, const CurveArray& target, bool cold);

    CurveModel* source = nullptr;

    StateRing<FilterState> states;
    IRBuilder      irBuilder;
    CurveFitter    fitter;
    BandMatcher    matcher;
    ConvolutionEngine partitioner;   // used only for its FFT, never for audio

    std::vector<float> irScratch;
    CurveArray target {}, achieved {};

    double sampleRate = 48000.0;
    int    partition  = 256;
    bool   prepared   = false;

    std::atomic<float> tilt { 0.0f }, smooth { 15.0f }, shift { 0.0f }, morphAmount { 0.0f };

    mutable std::mutex morphLock;
    CurveArray morphTarget {};
    std::uint64_t morphVersion = 0;
    std::atomic<int>   bandCount { 12 };
    std::atomic<int>   modeIndex { int (Mode::analog) };
    std::atomic<bool>  coldRequested { true };
    std::atomic<int>   latency { 0 };
    std::atomic<std::uint64_t> published { 0 };

    std::uint64_t lastVersion = 0;
    float lastTilt = 1.0e9f, lastSmooth = 1.0e9f, lastShift = 1.0e9f, lastMorph = 1.0e9f;
    std::uint64_t lastMorphVersion = 0;
    int   lastBandCount = -1, lastMode = -1;

    mutable std::mutex uiLock;
    UiSnapshot ui;

    std::thread thread;
    std::mutex wakeLock;
    std::condition_variable wake;
    std::atomic<bool> running { false };
};

} // namespace graphite
