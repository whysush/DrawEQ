#pragma once

#include <algorithm>
#include <cmath>

namespace graphite
{

/**
    The single frequency <-> index mapping used by everything: DSP, fitter, UI.

    CONTEXT.md 13.4 - nothing anywhere else is allowed to write `log (hz / 20)`.
    Two copies of this arithmetic drift apart the moment one of them is tweaked,
    and the symptom is a plot line that does not sit where the user drew.
*/
struct LogGrid
{
    static constexpr int   kSize  = 1024;
    static constexpr float kFMin  = 20.0f;      // Hz
    static constexpr float kFMax  = 20000.0f;   // Hz
    static constexpr float kMaxDb = 30.0f;      // curve clamp, +/-

    /** ln (kFMax / kFMin) and log2 of the same, as literals: std::log is not
        constexpr before C++26 and these must be usable in constant contexts.
        TestLogGrid asserts they match the runtime values. */
    static constexpr float kLnRatio   = 6.90775527898214f;   // ln 1000
    static constexpr float kLog2Ratio = 9.96578428466209f;   // log2 1000

    /** Grid index (may be fractional) -> Hz. */
    static float indexToHz (float index) noexcept
    {
        return kFMin * std::exp (kLnRatio * index / float (kSize - 1));
    }

    /** Hz -> grid index (may be fractional, and may fall outside [0, kSize-1]
        for frequencies beyond the drawn range - callers clamp if they need to). */
    static float hzToIndex (float hz) noexcept
    {
        return float (kSize - 1) * std::log (hz / kFMin) / kLnRatio;
    }

    /** 0 -> kFMin, 1 -> kFMax. The UI's x axis is this, scaled to the canvas. */
    static float hzToNorm (float hz) noexcept  { return hzToIndex (hz) / float (kSize - 1); }
    static float normToHz (float x)  noexcept  { return indexToHz (x * float (kSize - 1)); }

    /** A brush radius means the same thing at 60 Hz and 6 kHz because it is
        specified in octaves. This is the conversion that makes that true. */
    static float octavesToBins (float octaves) noexcept
    {
        return octaves * float (kSize - 1) / kLog2Ratio;
    }

    static float binsToOctaves (float bins) noexcept
    {
        return bins * kLog2Ratio / float (kSize - 1);
    }

    static float clampDb (float db) noexcept
    {
        return std::clamp (db, -kMaxDb, kMaxDb);
    }

    static float clampIndex (float i) noexcept
    {
        return std::clamp (i, 0.0f, float (kSize - 1));
    }

    static float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
    static float gainToDb (float g)  noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
};

} // namespace graphite
