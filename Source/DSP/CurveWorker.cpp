#include "CurveWorker.h"
#include "../Core/CurveShaping.h"
#include <chrono>

namespace draweq
{

CurveWorker::CurveWorker() = default;

CurveWorker::~CurveWorker()
{
    stop();
}

void CurveWorker::prepare (double sr, int maxBlockSize)
{
    const bool wasRunning = running.load (std::memory_order_acquire);

    if (wasRunning)
        stop();

    sampleRate = sr;
    partition  = ConvolutionEngine::partitionSizeFor (maxBlockSize);

    irBuilder.prepare (sr);
    fitter.prepare (sr);
    matcher.reset();
    partitioner.prepare (irBuilder.irLength(), partition, maxBlockSize);

    irScratch.assign (std::size_t (irBuilder.irLength()), 0.0f);

    states.resetAll();

    for (auto& entry : states.entries())
    {
        entry.allocate (irBuilder.irLength(), partition);
        entry.irLength      = irBuilder.irLength();
        entry.partitionSize = partition;
        entry.numPartitions = partitioner.numPartitions();
    }

    lastVersion   = 0;
    lastBandCount = -1;
    lastMode      = -1;
    lastTilt = lastSmooth = lastShift = 1.0e9f;
    coldRequested.store (true, std::memory_order_release);
    prepared = true;

    if (wasRunning)
        start();
}

void CurveWorker::start()
{
    if (running.exchange (true, std::memory_order_acq_rel))
        return;

    thread = std::thread ([this] { run(); });
}

void CurveWorker::stop()
{
    if (! running.exchange (false, std::memory_order_acq_rel))
        return;

    wake.notify_all();

    if (thread.joinable())
        thread.join();
}

void CurveWorker::setMacros (float tiltValue, float smoothPercent, float shiftValue,
                             int bands, Mode m) noexcept
{
    tilt.store (tiltValue, std::memory_order_relaxed);
    smooth.store (smoothPercent, std::memory_order_relaxed);
    shift.store (shiftValue, std::memory_order_relaxed);
    bandCount.store (bands, std::memory_order_relaxed);
    modeIndex.store (int (m), std::memory_order_relaxed);
}

void CurveWorker::setMorphTarget (const CurveArray& newTarget)
{
    const std::lock_guard<std::mutex> g (morphLock);
    morphTarget = newTarget;
    ++morphVersion;
}

CurveWorker::UiSnapshot CurveWorker::uiSnapshot() const
{
    const std::lock_guard<std::mutex> g (uiLock);
    return ui;
}

void CurveWorker::run()
{
    using namespace std::chrono_literals;

    while (running.load (std::memory_order_acquire))
    {
        {
            // A fixed tick rather than a wake-per-edit: this *is* the coalescing.
            std::unique_lock<std::mutex> lk (wakeLock);
            wake.wait_for (lk, 33ms, [this] { return ! running.load (std::memory_order_acquire); });
        }

        if (! running.load (std::memory_order_acquire))
            break;

        buildIfNeeded();
    }
}

void CurveWorker::fillCurrentSnapshot (CurveSnapshot& snap) const
{
    if (source == nullptr)
        return;

    source->fillSnapshot (snap);

    snap.tiltDbPerDecade    = tilt.load (std::memory_order_relaxed);
    snap.smoothOctaves      = shaping::smoothPercentToOctaves (smooth.load (std::memory_order_relaxed));
    snap.freqShiftSemitones = shift.load (std::memory_order_relaxed);
    snap.morphAmount        = morphAmount.load (std::memory_order_relaxed);
    snap.bandCount          = bandCount.load (std::memory_order_relaxed);
    snap.mode               = Mode (modeIndex.load (std::memory_order_relaxed));
    snap.sampleRate         = sampleRate;

    const std::lock_guard<std::mutex> g (morphLock);
    snap.morphTarget = snap.morphAmount > 1.0e-4f ? morphTarget : CurveArray {};
}

void CurveWorker::buildIfNeeded()
{
    if (! prepared || source == nullptr)
        return;

    // A stroke in progress leaves the filter alone. The curve is not finished
    // yet, so fitting it would be fitting something the user is still in the
    // middle of saying - and the fit that runs on release is a much better one
    // than anything affordable while they drag.
    const bool gestureOpen = source->isGestureOpen();

    if (gestureOpen && ! liveFit.load (std::memory_order_relaxed))
    {
        awaitingCommit.store (true, std::memory_order_release);
        return;
    }

    awaitingCommit.store (false, std::memory_order_release);

    // A finished gesture is a commit, whether or not this thread ever saw it
    // open. Counting completions rather than watching the flag is what makes a
    // flick shorter than one tick behave the same as a slow stroke.
    const std::uint64_t completions = source->gestureCount();

    const bool commit = completions != lastGestureCount
                      || deepRequested.exchange (false, std::memory_order_acq_rel);

    CurveSnapshot snap;
    fillCurrentSnapshot (snap);

    std::uint64_t currentMorphVersion = 0;

    {
        const std::lock_guard<std::mutex> g (morphLock);
        currentMorphVersion = morphVersion;
    }

    const bool cold = coldRequested.exchange (false, std::memory_order_acq_rel)
                   || snap.bandCount != lastBandCount
                   || int (snap.mode) != lastMode;

    // Exact equality would be the honest test here - "did this value move at
    // all" - but a macro arriving from a host's automation lane can wobble in
    // the last bit forever, and rebuilding for that would peg the worker.
    auto moved = [] (float a, float b) { return std::abs (a - b) > 1.0e-7f; };

    const bool changed = cold
                      || commit
                      || snap.version != lastVersion
                      || moved (snap.tiltDbPerDecade, lastTilt)
                      || moved (snap.smoothOctaves, lastSmooth)
                      || moved (snap.freqShiftSemitones, lastShift)
                      || moved (snap.morphAmount, lastMorph)
                      || currentMorphVersion != lastMorphVersion;

    if (! changed)
        return;

    FilterState* state = states.acquireFree();

    if (state == nullptr)
    {
        // All four entries are in flight, which means the audio thread has not
        // caught up. Drop this frame and try again on the next tick - the curve
        // will only be more current by then.
        // Leave lastGestureCount alone so the commit is retried next tick.
        coldRequested.store (cold, std::memory_order_release);
        deepRequested.store (commit, std::memory_order_release);
        return;
    }

    shaping::applyMacros (snap, target);

    state->mode          = snap.mode;
    state->sourceVersion = snap.version;

    if (snap.mode == Mode::analog)
        buildAnalog (*state, target, commit ? CurveFitter::Effort::deep
                                  : cold    ? CurveFitter::Effort::cold
                                            : CurveFitter::Effort::warm);
    else
        buildSpectral (*state, target, snap.mode == Mode::spectralMinimum);

    latency.store (state->latencySamples, std::memory_order_release);

    {
        const std::lock_guard<std::mutex> g (uiLock);
        ui.targetDb      = target;
        ui.achievedDb    = achieved;
        ui.bands         = state->bands;
        ui.numBands      = state->numBands;
        ui.trimDb        = state->trimDb;
        ui.mode          = snap.mode;
        ui.sourceVersion = snap.version;
        ui.valid         = true;
    }

    states.publish (state);
    published.fetch_add (1, std::memory_order_release);

    lastVersion   = snap.version;
    lastTilt      = snap.tiltDbPerDecade;
    lastSmooth    = snap.smoothOctaves;
    lastShift     = snap.freqShiftSemitones;
    lastMorph     = snap.morphAmount;
    lastMorphVersion = currentMorphVersion;
    lastGestureCount = completions;
    lastBandCount = snap.bandCount;
    lastMode      = int (snap.mode);
}

void CurveWorker::buildSpectral (FilterState& state, const CurveArray& t, bool minimumPhase)
{
    state.latencySamples = irBuilder.build (t, minimumPhase, irScratch.data());
    state.irLength       = irBuilder.irLength();
    state.partitionSize  = partition;
    state.numPartitions  = partitioner.numPartitions();
    state.numBands       = 0;
    state.trimDb         = 0.0f;

    partitioner.partitionIR (irScratch.data(), state.irLength, state.irSpectra.data());

    irBuilder.magnitudeResponse (irScratch.data(), state.irLength, achieved);

    float maxErr = 0.0f, sumSq = 0.0f;

    for (int i = 0; i < LogGrid::kSize; ++i)
    {
        const float e = std::abs (t[std::size_t (i)] - achieved[std::size_t (i)]);
        maxErr = std::max (maxErr, e);
        sumSq += e * e;
    }

    const std::lock_guard<std::mutex> g (uiLock);
    ui.maxErrorDb = maxErr;
    ui.rmsErrorDb = std::sqrt (sumSq / float (LogGrid::kSize));
}

void CurveWorker::buildAnalog (FilterState& state, const CurveArray& t, CurveFitter::Effort effort)
{
    fitter.setBellCount (bandCount.load (std::memory_order_relaxed));

    const auto& r = fitter.fit (t, effort);

    state.bands    = r.bands;
    state.numBands = r.numBands;
    state.trimDb   = r.gainDb;
    state.latencySamples = 0;

    matcher.match (state.bands.data(), fitter.bellCount(), state.numBands);

    for (int i = 0; i < LogGrid::kSize; ++i)
    {
        const float hz = LogGrid::indexToHz (float (i));
        achieved[std::size_t (i)] = r.gainDb
                                  + response::cascadeDb (state.bands.data(), state.numBands,
                                                         hz, sampleRate);
    }

    const std::lock_guard<std::mutex> g (uiLock);
    ui.maxErrorDb = r.maxErrorDb;
    ui.rmsErrorDb = r.rmsErrorDb;
}

} // namespace draweq
