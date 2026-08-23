#pragma once

#include "../Core/LogGrid.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace draweq
{

enum class BandType : int
{
    bell = 0,
    lowShelf,
    highShelf
};

/** One section of the cascade. Small, trivially copyable, safe to memcpy into
    a FilterState. */
struct Band
{
    BandType type   = BandType::bell;
    float    freqHz = 1000.0f;
    float    gainDb = 0.0f;
    float    q      = 0.707f;
    int      id     = 0;        // stable across fits; see BandMatcher
    bool     active = true;
};

/** N peaking bands (4-24) plus the two shelves. */
static constexpr int kMaxBells  = 24;
static constexpr int kMaxBands  = kMaxBells + 2;

/**
    Closed-form magnitude of exactly the filter BiquadCascade builds.

    This is not an approximation of the running filter, it is the same transfer
    function written out. The TPT SVF realises the bilinear transform of the
    analogue prototype with the cutoff prewarped at f0, so the digital magnitude
    at f is the analogue magnitude evaluated at

        x = tan (pi f / fs) / tan (pi f0 / fs)

    Because the fitter, the plot line and the audio path all come through here,
    the yellow line on screen is what you are hearing, not a redrawing of it.
*/
namespace response
{

inline float prewarp (float hz, double sampleRate) noexcept
{
    // Keep the tangent away from its pole; a band parked exactly at Nyquist is
    // not meaningful and would produce inf.
    const float nyquistLimit = float (sampleRate) * 0.4995f;
    return std::tan (3.14159265358979f * std::min (hz, nyquistLimit) / float (sampleRate));
}

/** dB magnitude of a single band at one frequency. */
inline float bandDb (const Band& b, float hz, double sampleRate) noexcept
{
    if (! b.active || std::abs (b.gainDb) < 1.0e-6f)
        return 0.0f;

    const float A  = std::pow (10.0f, b.gainDb * 0.025f);   // 10^(G/40)
    const float t  = prewarp (hz, sampleRate);
    const float t0 = prewarp (b.freqHz, sampleRate);
    const float omega = t / t0;

    float num = 1.0f, den = 1.0f;

    switch (b.type)
    {
        case BandType::bell:
        {
            const float x  = omega;
            const float x2 = x * x;
            const float s  = (1.0f - x2) * (1.0f - x2);
            const float p  = A / b.q;          // numerator damping
            const float qd = 1.0f / (A * b.q); // denominator damping
            num = s + p * p * x2;
            den = s + qd * qd * x2;
            break;
        }

        case BandType::lowShelf:
        {
            // The SVF prewarps the shelf at f0/sqrt(A), which shows up here as
            // the sqrt(A) scaling of the normalised frequency.
            const float x  = omega * std::sqrt (A);
            const float x2 = x * x;
            const float k  = 1.0f / b.q;
            const float A2 = A * A;
            num = (A2 - x2) * (A2 - x2) + k * k * A2 * x2;
            den = (1.0f - x2) * (1.0f - x2) + k * k * x2;
            break;
        }

        case BandType::highShelf:
        {
            const float x  = omega / std::sqrt (A);
            const float x2 = x * x;
            const float k  = 1.0f / b.q;
            const float A2 = A * A;
            num = (1.0f - A2 * x2) * (1.0f - A2 * x2) + k * k * A2 * x2;
            den = (1.0f - x2) * (1.0f - x2) + k * k * x2;
            break;
        }
    }

    return 10.0f * std::log10 (std::max (num, 1.0e-30f) / std::max (den, 1.0e-30f));
}

/** Cascade magnitude: sections multiply, so their dB add. */
inline float cascadeDb (const Band* bands, int n, float hz, double sampleRate) noexcept
{
    float acc = 0.0f;

    for (int i = 0; i < n; ++i)
        acc += bandDb (bands[i], hz, sampleRate);

    return acc;
}

/**
    Analytic gradient of a bell's dB magnitude with respect to
    (ln f0, G in dB, ln Q) - the coordinates the fitter optimises in, so that a
    step means the same perceptual distance at 40 Hz as at 12 kHz.

    Writing d/dtheta of 10*log10 (N/D) as (10/ln10) * (N'/N - D'/D) and pushing
    the chain rule through x = tan(pi f/fs) / tan(pi f0/fs):

        dL/dlnf0 = (10/ln10) (Nx/N - Dx/D) * (-x * c)   with c = (pi f0/fs)(1+t0^2)/t0
        dL/dG    = (x^2 / 2) (p^2/N + q^2/D)
        dL/dlnQ  = (20/ln10) x^2 (q^2/D - p^2/N)

    Roughly 6x faster than finite differences and far better conditioned, which
    is what lets the warm-start fit run inside 2 ms.
*/
inline void bellGradient (const Band& b, float hz, double sampleRate,
                          float& dLogF, float& dGain, float& dLogQ) noexcept
{
    const float A  = std::pow (10.0f, b.gainDb * 0.025f);
    const float t  = prewarp (hz, sampleRate);
    const float t0 = prewarp (b.freqHz, sampleRate);
    const float x  = t / t0;
    const float x2 = x * x;

    const float s  = (1.0f - x2) * (1.0f - x2);
    const float p  = A / b.q;
    const float qd = 1.0f / (A * b.q);

    const float N = std::max (s + p * p * x2,  1.0e-30f);
    const float D = std::max (s + qd * qd * x2, 1.0e-30f);

    const float dNdx = -4.0f * x * (1.0f - x2) + 2.0f * p  * p  * x;
    const float dDdx = -4.0f * x * (1.0f - x2) + 2.0f * qd * qd * x;

    constexpr float tenOverLn10 = 4.34294481903252f;   // 10 / ln 10

    const float w  = 3.14159265358979f * b.freqHz / float (sampleRate);
    const float c  = w * (1.0f + t0 * t0) / std::max (t0, 1.0e-12f);

    dLogF = tenOverLn10 * (dNdx / N - dDdx / D) * (-x * c);
    dGain = 0.5f * x2 * (p * p / N + qd * qd / D);
    dLogQ = 2.0f * tenOverLn10 * x2 * (qd * qd / D - p * p / N);
}

/**
    The same maths with everything that depends only on the band hoisted out.

    `bandDb` is the readable reference and the thing the tests check against;
    this is what the fitter's inner loops call. It matters because a warm-start
    frame evaluates 14 bands at 256 points up to 30 times, and calling tan, pow
    and log for every one of those pairs is most of the frame budget. The
    frequency-dependent part reduces to one multiply once tan (pi f / fs) is
    tabulated per evaluation point.

    TestBandResponse asserts the two agree to a thousandth of a dB.
*/
struct BandEval
{
    BandType type   = BandType::bell;
    float    xScale = 0.0f;    // multiply tan (pi f / fs) by this to get x
    float    A2     = 1.0f;    // A^2
    float    A4     = 1.0f;    // A^4, the shelf numerator constant
    float    kA2    = 0.0f;    // (k A)^2
    float    k2     = 0.0f;    // k^2
    float    p2     = 0.0f;    // bell numerator damping, squared
    float    q2     = 0.0f;    // bell denominator damping, squared
    float    cFreq  = 0.0f;    // d ln x / d ln f0, magnitude
    bool     unity  = true;

    static BandEval make (const Band& b, double sampleRate) noexcept
    {
        BandEval e;
        e.type = b.type;

        // The constants are computed even for a band sitting at 0 dB. db()
        // short-circuits on `unity`, but gradient() must not: the derivative of
        // a flat band with respect to its gain is exactly the shape the fitter
        // needs in order to recruit it, and zeroing that column would leave
        // every unused band permanently unusable.
        e.unity = ! b.active || std::abs (b.gainDb) < 1.0e-6f;

        const float A  = std::pow (10.0f, b.gainDb * 0.025f);
        const float t0 = prewarp (b.freqHz, sampleRate);
        const float invT0 = 1.0f / t0;
        const float sqrtA = std::sqrt (A);

        e.A2 = A * A;
        e.A4 = e.A2 * e.A2;

        switch (b.type)
        {
            case BandType::bell:
            {
                e.xScale = invT0;
                const float p = A / b.q;
                const float q = 1.0f / (A * b.q);
                e.p2 = p * p;
                e.q2 = q * q;
                break;
            }

            case BandType::lowShelf:
                e.xScale = invT0 * sqrtA;
                e.k2  = 1.0f / (b.q * b.q);
                e.kA2 = e.k2 * e.A2;
                break;

            case BandType::highShelf:
                e.xScale = invT0 / sqrtA;
                e.k2  = 1.0f / (b.q * b.q);
                e.kA2 = e.k2 * e.A2;
                break;
        }

        const float w = 3.14159265358979f * b.freqHz / float (sampleRate);
        e.cFreq = w * (1.0f + t0 * t0) * invT0;

        return e;
    }

    /** `tanF` is tan (pi f / fs) for the evaluation point, tabulated once. */
    float db (float tanF) const noexcept
    {
        if (unity)
            return 0.0f;

        const float x  = tanF * xScale;
        const float x2 = x * x;
        const float s  = (1.0f - x2) * (1.0f - x2);

        float num, den;

        switch (type)
        {
            case BandType::bell:
                num = s + p2 * x2;
                den = s + q2 * x2;
                break;

            case BandType::lowShelf:
                num = (A2 - x2) * (A2 - x2) + kA2 * x2;
                den = s + k2 * x2;
                break;

            case BandType::highShelf:
            default:
                num = (1.0f - A2 * x2) * (1.0f - A2 * x2) + kA2 * x2;
                den = s + k2 * x2;
                break;
        }

        // One log instead of two log10 calls; 10/ln10 folds the base change in.
        return 4.34294481903252f * std::log (std::max (num, 1.0e-30f) / std::max (den, 1.0e-30f));
    }

    /** Bell gradient in (ln f0, G, ln Q), same derivation as bellGradient. */
    void gradient (float tanF, float& dLogF, float& dGain, float& dLogQ) const noexcept
    {
        const float x  = tanF * xScale;
        const float x2 = x * x;
        const float s  = (1.0f - x2) * (1.0f - x2);

        const float N = std::max (s + p2 * x2, 1.0e-30f);
        const float D = std::max (s + q2 * x2, 1.0e-30f);

        const float common = -4.0f * x * (1.0f - x2);
        const float dNdx = common + 2.0f * p2 * x;
        const float dDdx = common + 2.0f * q2 * x;

        constexpr float tenOverLn10 = 4.34294481903252f;

        dLogF = tenOverLn10 * (dNdx / N - dDdx / D) * (-x * cFreq);
        dGain = 0.5f * x2 * (p2 / N + q2 / D);
        dLogQ = 2.0f * tenOverLn10 * x2 * (q2 / D - p2 / N);
    }
};

} // namespace response
} // namespace draweq
