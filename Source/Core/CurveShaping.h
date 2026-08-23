#pragma once

#include "CurveSnapshot.h"
#include <cmath>

namespace draweq
{

/**
    The macro chain that turns the raw stroke into the curve the DSP actually
    targets: shift -> tilt -> smooth.

    Order matters. Shift first, because sliding a stroke should carry its shape
    intact. Tilt second, because a tilt is an absolute statement about the
    spectrum, not something that should slide with the stroke. Smooth last, so
    it also softens any corner the first two introduced.

    All of it is pure and header-only: the worker runs it, and the UI runs it to
    draw the ghost, and both must agree exactly.
*/
namespace shaping
{

/** Slide the whole curve along the log axis. Edges hold their end value, which
    is what CONTEXT.md 6.1 specifies for out-of-range frequencies. */
inline void applyFreqShift (const CurveArray& in, CurveArray& out, float semitones) noexcept
{
    if (std::abs (semitones) < 1.0e-4f)
    {
        out = in;
        return;
    }

    const float shiftBins = LogGrid::octavesToBins (semitones / 12.0f);

    for (int i = 0; i < LogGrid::kSize; ++i)
    {
        const float src = float (i) - shiftBins;
        const float c   = LogGrid::clampIndex (src);
        const int   i0  = int (c);
        const int   i1  = std::min (i0 + 1, LogGrid::kSize - 1);
        out[size_t (i)] = in[size_t (i0)] + (c - float (i0)) * (in[size_t (i1)] - in[size_t (i0)]);
    }
}

/** dB per decade, pivoting at 1 kHz so the tilt does not also change level. */
inline void applyTilt (CurveArray& c, float dbPerDecade) noexcept
{
    if (std::abs (dbPerDecade) < 1.0e-4f)
        return;

    const float pivot = LogGrid::hzToIndex (1000.0f);
    const float decadesPerBin = LogGrid::kLnRatio / (std::log (10.0f) * float (LogGrid::kSize - 1));

    for (int i = 0; i < LogGrid::kSize; ++i)
        c[size_t (i)] = LogGrid::clampDb (c[size_t (i)]
                                          + dbPerDecade * (float (i) - pivot) * decadesPerBin);
}

/** Gaussian along the log axis, so the blur is a constant width in octaves at
    every frequency. Non-destructive: the caller keeps the raw array. */
inline void applyGaussianSmoothing (const CurveArray& in, CurveArray& out, float sigmaOctaves) noexcept
{
    const float sigmaBins = LogGrid::octavesToBins (sigmaOctaves);

    if (sigmaBins < 0.5f)
    {
        out = in;
        return;
    }

    const int   radius = std::min (int (std::ceil (3.0f * sigmaBins)), LogGrid::kSize - 1);
    const float inv2s2 = 1.0f / (2.0f * sigmaBins * sigmaBins);

    // Kernel is separable-free (1-D) and small enough that a direct convolution
    // beats an FFT at every sigma we allow.
    for (int i = 0; i < LogGrid::kSize; ++i)
    {
        float acc = 0.0f, norm = 0.0f;

        for (int k = -radius; k <= radius; ++k)
        {
            const int   j = std::clamp (i + k, 0, LogGrid::kSize - 1);   // edges hold
            const float w = std::exp (-float (k) * float (k) * inv2s2);
            acc  += w * in[size_t (j)];
            norm += w;
        }

        out[size_t (i)] = acc / norm;
    }
}

/** smooth parameter (0-100 %) -> sigma in octaves (CONTEXT.md 8.2). */
inline float smoothPercentToOctaves (float percent) noexcept
{
    return 0.015f * percent;   // 100 % -> 1.5 octaves
}

/** The full chain. `out` is what the DSP targets and what the ghost draws. */
inline void applyMacros (const CurveSnapshot& s, CurveArray& out)
{
    CurveArray base = s.raw;

    // At morph = 0 the live curve passes through untouched, so there is no jump
    // as the control leaves zero - the thing that makes automating it usable.
    if (s.morphAmount > 1.0e-4f)
    {
        const float t = std::min (s.morphAmount, 1.0f);

        for (std::size_t i = 0; i < base.size(); ++i)
            base[i] += t * (s.morphTarget[i] - base[i]);
    }

    CurveArray shifted;
    applyFreqShift (base, shifted, s.freqShiftSemitones);
    applyTilt (shifted, s.tiltDbPerDecade);
    applyGaussianSmoothing (shifted, out, s.smoothOctaves);

    for (auto& v : out)
        v = LogGrid::clampDb (v);
}

} // namespace shaping
} // namespace draweq
