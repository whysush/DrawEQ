#include "CurveFitter.h"
#include "../Core/LogGrid.h"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace graphite
{

namespace
{
    constexpr double kMinQ = 0.1, kMaxQ = 18.0;
    constexpr double kMaxG = 30.0;

    inline int bandBase (int band) { return 1 + 3 * band; }
}

void CurveFitter::prepare (double sampleRate)
{
    sr = sampleRate;

    for (int i = 0; i < kFitPoints; ++i)
    {
        const float gridIndex = float (i) * float (LogGrid::kSize - 1) / float (kFitPoints - 1);
        freqs[size_t (i)] = LogGrid::indexToHz (gridIndex);

        // Tabulating the prewarp is what takes a warm frame from milliseconds
        // to microseconds: it is the only transcendental in the evaluation that
        // depends on the point rather than the band.
        tanF[size_t (i)] = response::prewarp (freqs[size_t (i)], sampleRate);
    }

    setBellCount (numBells);
    reset();
}

void CurveFitter::setBellCount (int n)
{
    const int clamped = std::clamp (n, 4, kMaxBells);

    if (clamped == numBells && ! theta.empty())
        return;

    numBells  = clamped;
    numBands  = numBells + 2;
    numParams = 1 + 3 * numBands;

    theta.assign (size_t (numParams), 0.0);
    thetaBest.assign (size_t (numParams), 0.0);
    thetaTrial.assign (size_t (numParams), 0.0);
    model.assign (kFitPoints, 0.0);
    residual.assign (kFitPoints, 0.0);
    scratch.assign (kFitPoints, 0.0);
    jacobian.assign (size_t (kFitPoints) * size_t (numParams), 0.0);

    // The two shelves always occupy the last two slots, so slot indices stay
    // meaningful when the bell count changes.
    for (int b = 0; b < numBands; ++b)
    {
        work[size_t (b)].type = b < numBells ? BandType::bell
                                             : (b == numBells ? BandType::lowShelf
                                                              : BandType::highShelf);
        work[size_t (b)].id     = b;
        work[size_t (b)].active = true;
    }

    haveWarmStart = false;
}

void CurveFitter::reset()
{
    haveWarmStart = false;
    std::fill (theta.begin(), theta.end(), 0.0);
}

void CurveFitter::buildTargets (const CurveArray& target)
{
    for (int i = 0; i < kFitPoints; ++i)
    {
        const float gridIndex = float (i) * float (LogGrid::kSize - 1) / float (kFitPoints - 1);
        const int   i0 = int (gridIndex);
        const int   i1 = std::min (i0 + 1, LogGrid::kSize - 1);
        const float t  = gridIndex - float (i0);
        targetDb[size_t (i)] = double (target[size_t (i0)] + t * (target[size_t (i1)] - target[size_t (i0)]));
    }
}

// ---------------------------------------------------------------------------
// Seeding
// ---------------------------------------------------------------------------

void CurveFitter::seedSpread()
{
    // Fallback seed: bands spread evenly across the log axis with no gain. LM
    // can always recruit one, it just takes more iterations than a good seed.
    theta[0] = std::accumulate (targetDb.begin(), targetDb.end(), 0.0) / double (kFitPoints);

    for (int b = 0; b < numBells; ++b)
    {
        const double t = (double (b) + 0.5) / double (numBells);
        theta[size_t (bandBase (b) + 0)] = std::log (double (LogGrid::normToHz (float (t))));
        theta[size_t (bandBase (b) + 1)] = 0.0;
        theta[size_t (bandBase (b) + 2)] = std::log (1.4);
    }

    theta[size_t (bandBase (numBells) + 0)] = std::log (100.0);
    theta[size_t (bandBase (numBells) + 1)] = 0.0;
    theta[size_t (bandBase (numBells) + 2)] = std::log (0.707);

    theta[size_t (bandBase (numBells + 1) + 0)] = std::log (8000.0);
    theta[size_t (bandBase (numBells + 1) + 1)] = 0.0;
    theta[size_t (bandBase (numBells + 1) + 2)] = std::log (0.707);
}

void CurveFitter::seedCold()
{
    seedSpread();

    // 1. Broadband level and slope. The slope is not written to the user's tilt
    //    control - that macro is an input to the target, and writing to it would
    //    make the fitter chase its own tail. It only seeds the shelves, which is
    //    what a broadband tilt actually looks like in a biquad cascade.
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;

    for (int i = 0; i < kFitPoints; ++i)
    {
        const double x = double (i) / double (kFitPoints - 1);
        sx += x; sy += targetDb[size_t (i)];
        sxx += x * x; sxy += x * targetDb[size_t (i)];
    }

    const double n     = double (kFitPoints);
    const double denom = n * sxx - sx * sx;
    const double slope = std::abs (denom) > 1.0e-12 ? (n * sxy - sx * sy) / denom : 0.0;
    const double inter = (sy - slope * sx) / n;

    theta[0] = inter + slope * 0.5;    // level at the middle of the range

    std::array<double, kFitPoints> residualDb {};

    for (int i = 0; i < kFitPoints; ++i)
        residualDb[size_t (i)] = targetDb[size_t (i)] - theta[0];

    // 2. Shelves take the ends.
    auto meanOver = [&] (float f0, float f1)
    {
        double acc = 0.0; int count = 0;

        for (int i = 0; i < kFitPoints; ++i)
            if (freqs[size_t (i)] >= f0 && freqs[size_t (i)] <= f1)
            {
                acc += residualDb[size_t (i)];
                ++count;
            }

        return count > 0 ? acc / double (count) : 0.0;
    };

    const double lowGain  = std::clamp (meanOver (20.0f, 70.0f),      -kMaxG, kMaxG);
    const double highGain = std::clamp (meanOver (11000.0f, 20000.0f), -kMaxG, kMaxG);

    theta[size_t (bandBase (numBells) + 1)]     = lowGain;
    theta[size_t (bandBase (numBells + 1) + 1)] = highGain;

    for (int b = numBells; b < numBands; ++b)
    {
        work[size_t (b)].freqHz = float (std::exp (theta[size_t (bandBase (b))]));
        work[size_t (b)].gainDb = float (theta[size_t (bandBase (b) + 1)]);
        work[size_t (b)].q      = float (std::exp (theta[size_t (bandBase (b) + 2)]));

        const auto eval = response::BandEval::make (work[size_t (b)], sr);

        for (int i = 0; i < kFitPoints; ++i)
            residualDb[size_t (i)] -= double (eval.db (tanF[size_t (i)]));
    }

    // 3. Peak-pick what is left. Score by |amplitude| * width so a broad 3 dB
    //    hump outranks a one-point 6 dB spike: the spike is probably not
    //    fittable anyway, and spending a band on it starves the rest.
    struct Peak { int index; double amp; double widthOct; double score; };
    std::vector<Peak> peaks;
    peaks.reserve (32);

    for (int i = 1; i < kFitPoints - 1; ++i)
    {
        const double a = residualDb[size_t (i)];
        const bool isMax = a > residualDb[size_t (i - 1)] && a >= residualDb[size_t (i + 1)] && a > 0.5;
        const bool isMin = a < residualDb[size_t (i - 1)] && a <= residualDb[size_t (i + 1)] && a < -0.5;

        if (! isMax && ! isMin)
            continue;

        // Walk out to the half-amplitude points: that is the -3 dB width the
        // seed Q comes from.
        const double halfLevel = a * 0.5;
        int lo = i, hi = i;

        while (lo > 0 && std::abs (residualDb[size_t (lo)]) > std::abs (halfLevel)) --lo;
        while (hi < kFitPoints - 1 && std::abs (residualDb[size_t (hi)]) > std::abs (halfLevel)) ++hi;

        const double widthOct = std::max (0.05,
            std::log2 (double (freqs[size_t (hi)]) / double (freqs[size_t (lo)])));

        peaks.push_back ({ i, a, widthOct, std::abs (a) * widthOct });
    }

    std::sort (peaks.begin(), peaks.end(),
               [] (const Peak& a, const Peak& b) { return a.score > b.score; });

    int assigned = 0;

    for (const auto& p : peaks)
    {
        if (assigned >= numBells)
            break;

        // Q from bandwidth in octaves, the standard RBJ relation.
        const double bw = p.widthOct;
        const double q  = std::clamp (std::sqrt (std::pow (2.0, bw)) / (std::pow (2.0, bw) - 1.0),
                                      kMinQ, kMaxQ);

        theta[size_t (bandBase (assigned) + 0)] = std::log (double (freqs[size_t (p.index)]));
        theta[size_t (bandBase (assigned) + 1)] = std::clamp (p.amp, -kMaxG, kMaxG);
        theta[size_t (bandBase (assigned) + 2)] = std::log (q);
        ++assigned;
    }

    // Unassigned bells sit flat, spread across the range, ready to be recruited.
    for (int b = assigned; b < numBells; ++b)
    {
        const double t = (double (b - assigned) + 0.5) / double (std::max (1, numBells - assigned));
        theta[size_t (bandBase (b) + 0)] = std::log (double (LogGrid::normToHz (float (t))));
        theta[size_t (bandBase (b) + 1)] = 0.0;
        theta[size_t (bandBase (b) + 2)] = std::log (1.4);
    }
}

// ---------------------------------------------------------------------------
// Model, residual, constraints
// ---------------------------------------------------------------------------

void CurveFitter::evaluateModel (std::vector<double>& out) const
{
    std::fill (out.begin(), out.end(), theta[0]);

    for (int b = 0; b < numBands; ++b)
    {
        Band band = work[size_t (b)];
        band.freqHz = float (std::exp (theta[size_t (bandBase (b) + 0)]));
        band.gainDb = float (theta[size_t (bandBase (b) + 1)]);
        band.q      = float (std::exp (theta[size_t (bandBase (b) + 2)]));

        if (std::abs (band.gainDb) < 1.0e-6f)
            continue;

        const auto eval = response::BandEval::make (band, sr);

        for (int i = 0; i < kFitPoints; ++i)
            out[size_t (i)] += double (eval.db (tanF[size_t (i)]));
    }
}

double CurveFitter::residualNorm (std::vector<double>& out)
{
    evaluateModel (out);

    double sum = 0.0;

    for (int i = 0; i < kFitPoints; ++i)
    {
        const double e = targetDb[size_t (i)] - out[size_t (i)];
        sum += e * e;
    }

    return sum;
}

void CurveFitter::project()
{
    const double fMax = std::min (20000.0, 0.45 * sr);

    theta[0] = std::clamp (theta[0], -kMaxG, kMaxG);

    for (int b = 0; b < numBands; ++b)
    {
        double& lf = theta[size_t (bandBase (b) + 0)];
        double& g  = theta[size_t (bandBase (b) + 1)];
        double& lq = theta[size_t (bandBase (b) + 2)];

        lf = std::clamp (lf, std::log (20.0), std::log (fMax));
        g  = std::clamp (g, -kMaxG, kMaxG);
        lq = std::clamp (lq, std::log (kMinQ), std::log (kMaxQ));
    }
}

// ---------------------------------------------------------------------------
// Levenberg-Marquardt
// ---------------------------------------------------------------------------

double CurveFitter::runLM (int maxIterations)
{
    using Eigen::MatrixXd;
    using Eigen::VectorXd;

    double lambda = 1.0e-3;
    double error  = residualNorm (model);

    Eigen::Map<MatrixXd> J (jacobian.data(), kFitPoints, numParams);

    for (int iter = 0; iter < maxIterations; ++iter)
    {
        result.iterations = iter + 1;

        for (int i = 0; i < kFitPoints; ++i)
            residual[size_t (i)] = targetDb[size_t (i)] - model[size_t (i)];

        // --- Jacobian ---
        for (int i = 0; i < kFitPoints; ++i)
            J (i, 0) = 1.0;   // the broadband trim shifts every point equally

        for (int b = 0; b < numBands; ++b)
        {
            Band band = work[size_t (b)];
            band.freqHz = float (std::exp (theta[size_t (bandBase (b) + 0)]));
            band.gainDb = float (theta[size_t (bandBase (b) + 1)]);
            band.q      = float (std::exp (theta[size_t (bandBase (b) + 2)]));

            const int c = bandBase (b);

            if (band.type == BandType::bell)
            {
                const auto eval = response::BandEval::make (band, sr);

                for (int i = 0; i < kFitPoints; ++i)
                {
                    float df = 0.0f, dg = 0.0f, dq = 0.0f;
                    eval.gradient (tanF[size_t (i)], df, dg, dq);
                    J (i, c + 0) = double (df);
                    J (i, c + 1) = double (dg);
                    J (i, c + 2) = double (dq);
                }
            }
            else
            {
                // Shelves are two of the twenty-six sections and their closed
                // form is messier than the bell's; central differences here cost
                // six extra evaluations per iteration and are not measurable.
                constexpr double h = 1.0e-4;

                for (int p = 0; p < 3; ++p)
                {
                    Band plus = band, minus = band;
                    auto apply = [&] (Band& t, double delta)
                    {
                        if (p == 0) t.freqHz = float (std::exp (theta[size_t (c)] + delta));
                        if (p == 1) t.gainDb = float (theta[size_t (c + 1)] + delta);
                        if (p == 2) t.q      = float (std::exp (theta[size_t (c + 2)] + delta));
                    };

                    apply (plus, h);
                    apply (minus, -h);

                    const auto ep = response::BandEval::make (plus, sr);
                    const auto em = response::BandEval::make (minus, sr);

                    for (int i = 0; i < kFitPoints; ++i)
                        J (i, c + p) = (double (ep.db (tanF[size_t (i)]))
                                      - double (em.db (tanF[size_t (i)]))) / (2.0 * h);
                }
            }
        }

        Eigen::Map<VectorXd> r (residual.data(), kFitPoints);

        const MatrixXd JtJ = J.transpose() * J;
        const VectorXd Jtr = J.transpose() * r;

        bool improved = false;

        // Up to five damping attempts per iteration: a rejected step is cheap
        // (one model evaluation) compared to rebuilding the Jacobian.
        for (int attempt = 0; attempt < 5 && ! improved; ++attempt)
        {
            MatrixXd A = JtJ;

            for (int k = 0; k < numParams; ++k)
                A (k, k) += lambda * std::max (JtJ (k, k), 1.0e-6);

            const VectorXd delta = A.ldlt().solve (Jtr);

            if (! delta.allFinite())
            {
                lambda *= 10.0;
                continue;
            }

            thetaTrial = theta;

            for (int k = 0; k < numParams; ++k)
                theta[size_t (k)] += delta (k);

            project();

            const double trialError = residualNorm (scratch);

            if (trialError < error)
            {
                error   = trialError;
                model   = scratch;
                lambda  = std::max (lambda * 0.3, 1.0e-9);
                improved = true;
            }
            else
            {
                theta = thetaTrial;
                lambda *= 10.0;
            }
        }

        if (! improved)
            break;   // damping has run out of road; more iterations will not help
    }

    return error;
}

// ---------------------------------------------------------------------------
// Driving
// ---------------------------------------------------------------------------

void CurveFitter::writeResult()
{
    result.gainDb   = float (theta[0]);
    result.numBands = numBands;

    for (int b = 0; b < numBands; ++b)
    {
        Band& band = result.bands[size_t (b)];
        band        = work[size_t (b)];
        band.freqHz = float (std::exp (theta[size_t (bandBase (b) + 0)]));
        band.gainDb = float (theta[size_t (bandBase (b) + 1)]);
        band.q      = float (std::exp (theta[size_t (bandBase (b) + 2)]));
        band.id     = b;
        band.active = true;
    }

    for (int b = numBands; b < kMaxBands; ++b)
        result.bands[size_t (b)] = Band { BandType::bell, 1000.0f, 0.0f, 1.0f, b, false };

    evaluateModel (model);

    double maxErr = 0.0, sumSq = 0.0;

    for (int i = 0; i < kFitPoints; ++i)
    {
        const double e = std::abs (targetDb[size_t (i)] - model[size_t (i)]);
        maxErr = std::max (maxErr, e);
        sumSq += e * e;
    }

    result.maxErrorDb = float (maxErr);
    result.rmsErrorDb = float (std::sqrt (sumSq / double (kFitPoints)));
}

/** Deterministic jitter around the best solution so far.

    Deterministic on purpose: a fit that returns a different answer each time it
    is run on the same curve would make the regression corpus meaningless and
    would be miserable to debug. The sequence is a fixed hash of the variant
    index and the parameter index, not a random number generator. */
void CurveFitter::perturbFromBest (unsigned int variant)
{
    theta = thetaBest;

    for (int b = 0; b < numBands; ++b)
    {
        const int c = bandBase (b);

        auto jitter = [variant, c] (int k, double scale)
        {
            unsigned int h = variant * 2654435761u + (unsigned int) (c + k) * 2246822519u;
            h ^= h >> 13;
            h *= 3266489917u;
            h ^= h >> 16;
            return (double (h & 0xffffu) / 32768.0 - 1.0) * scale;   // -scale .. +scale
        };

        theta[size_t (c + 0)] += jitter (0, 0.35);   // ln f: about a third of an octave
        theta[size_t (c + 1)] += jitter (1, 3.0);    // dB
        theta[size_t (c + 2)] += jitter (2, 0.40);   // ln Q
    }

    project();
}

const CurveFitter::Result& CurveFitter::fit (const CurveArray& target, Effort effort)
{
    buildTargets (target);

    if (effort == Effort::warm && ! haveWarmStart)
        effort = Effort::cold;

    if (effort == Effort::warm)
    {
        // The curve moved a little, so the solution moved a little.
        runLM (5);
    }
    else
    {
        const int iterations = effort == Effort::deep ? 150 : 40;

        // Two seeds, better one wins. A peak-picked seed is nearly always
        // ahead, but a target with no clear extrema (a pure tilt) is a case
        // where the spread seed converges to a better basin.
        seedCold();
        double bestError = runLM (iterations);
        thetaBest = theta;

        seedSpread();
        const double spreadError = runLM (iterations);

        if (spreadError < bestError)
        {
            bestError = spreadError;
            thetaBest = theta;
        }

        if (effort == Effort::deep)
        {
            // Perturbed restarts. Levenberg-Marquardt only ever walks downhill,
            // so on a curve with more structure than bands it settles into
            // whichever basin the seed happened to land in. Shaking the best
            // solution and re-converging is what finds the better basin, and it
            // is only affordable because this runs once per stroke rather than
            // thirty times a second.
            for (unsigned int variant = 1; variant <= 3; ++variant)
            {
                perturbFromBest (variant);
                const double e = runLM (80);

                if (e < bestError)
                {
                    bestError = e;
                    thetaBest = theta;
                }
            }
        }

        theta = thetaBest;
        haveWarmStart = true;
    }

    writeResult();
    return result;
}

} // namespace graphite
