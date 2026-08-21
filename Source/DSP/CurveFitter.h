#pragma once

#include "../Core/CurveSnapshot.h"
#include "BandResponse.h"
#include <array>
#include <vector>

namespace graphite
{

/**
    Fits a cascade of biquads to the drawn curve: Levenberg-Marquardt over
    (log f, G, log Q) plus a broadband trim.

    Why those coordinates: a step of 0.05 means the same perceptual distance at
    40 Hz as at 12 kHz only if frequency is optimised logarithmically, and the
    same is true of Q. Optimising raw Hz makes the solver spend its whole budget
    in the top octave.

    Why the broadband trim: bells and shelves cannot express a constant offset,
    so a target of "+4 dB everywhere" would otherwise be fit by two shelves
    fighting each other. One extra scalar removes a whole class of bad fits and
    is free to apply (CONTEXT.md deviates here; see DECISIONS.md).

    Warm start is the reason this can run live. Between two frames of a drag the
    curve barely moves, so starting from the previous solution and running a
    handful of iterations lands in tens of microseconds. A cold start - preset
    load, paste, band-count change - gets a much larger budget and two seeds.

    Worker thread only. Allocates in prepare(), never in fit().
*/
class CurveFitter
{
public:
    struct Result
    {
        std::array<Band, kMaxBands> bands {};
        int   numBands   = 0;
        float gainDb     = 0.0f;      // broadband trim
        float maxErrorDb = 0.0f;
        float rmsErrorDb = 0.0f;
        int   iterations = 0;
    };

    /** Points the error is evaluated at. 256 on the log grid is dense enough
        that nothing hides between samples and small enough that a warm-start
        solve is a rounding error in the frame budget. */
    static constexpr int kFitPoints = 256;

    void prepare (double sampleRate);

    /** Changing the band count invalidates the warm start. */
    void setBellCount (int n);
    int  bellCount() const noexcept { return numBells; }

    /** Forget the previous solution; the next fit will be cold. */
    void reset();

    /** Runs a fit against `target` (dB on the log grid) and returns the result.
        `cold` forces a full re-seed even if a warm start is available. */
    const Result& fit (const CurveArray& target, bool cold);

    const Result& lastResult() const noexcept { return result; }

private:
    void   buildTargets (const CurveArray& target);
    void   seedCold();
    void   seedSpread();
    double runLM (int maxIterations);
    void   evaluateModel (std::vector<double>& out) const;
    double residualNorm (std::vector<double>& scratch);
    void   project();
    void   writeResult();

    double sr = 48000.0;
    int    numBells = 12;
    int    numBands = 14;     // bells + two shelves
    int    numParams = 1 + 3 * 14;
    bool   haveWarmStart = false;

    std::array<float, kFitPoints> freqs {};
    std::array<float, kFitPoints> tanF {};      // tan (pi f / fs), tabulated once
    std::array<double, kFitPoints> targetDb {};

    // theta layout: [trim, (logF, G, logQ) * numBands], shelves last.
    std::vector<double> theta, thetaBest, thetaTrial;
    std::vector<double> model, residual, scratch;
    std::vector<double> jacobian;   // kFitPoints x numParams, row major

    std::array<Band, kMaxBands> work {};

    Result result;
};

} // namespace graphite
