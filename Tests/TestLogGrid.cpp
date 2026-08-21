#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Core/LogGrid.h"
#include "Core/CurveShaping.h"

using namespace graphite;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE ("LogGrid constants match their runtime values", "[loggrid]")
{
    // The literals exist because std::log is not constexpr. If they ever drift,
    // the UI and the DSP place the same frequency in two different places.
    REQUIRE_THAT (double (LogGrid::kLnRatio),
                  WithinRel (std::log (double (LogGrid::kFMax / LogGrid::kFMin)), 1.0e-6));
    REQUIRE_THAT (double (LogGrid::kLog2Ratio),
                  WithinRel (std::log2 (double (LogGrid::kFMax / LogGrid::kFMin)), 1.0e-6));
}

TEST_CASE ("index <-> Hz round-trips", "[loggrid]")
{
    for (float hz : { 20.0f, 31.5f, 100.0f, 440.0f, 1000.0f, 6300.0f, 20000.0f })
        REQUIRE_THAT (double (LogGrid::indexToHz (LogGrid::hzToIndex (hz))),
                      WithinRel (double (hz), 1.0e-4));

    REQUIRE_THAT (double (LogGrid::indexToHz (0.0f)), WithinRel (20.0, 1.0e-5));
    REQUIRE_THAT (double (LogGrid::indexToHz (float (LogGrid::kSize - 1))), WithinRel (20000.0, 1.0e-5));
}

TEST_CASE ("an octave is the same number of bins everywhere", "[loggrid]")
{
    // This is the property that makes a brush radius in octaves feel identical
    // at 60 Hz and 6 kHz.
    const float lowSpan  = LogGrid::hzToIndex (120.0f) - LogGrid::hzToIndex (60.0f);
    const float highSpan = LogGrid::hzToIndex (12000.0f) - LogGrid::hzToIndex (6000.0f);

    REQUIRE_THAT (double (lowSpan), WithinRel (double (highSpan), 1.0e-4));
    REQUIRE_THAT (double (LogGrid::octavesToBins (1.0f)), WithinRel (double (lowSpan), 1.0e-4));
    REQUIRE_THAT (double (LogGrid::binsToOctaves (LogGrid::octavesToBins (2.5f))),
                  WithinRel (2.5, 1.0e-5));
}

TEST_CASE ("frequency shift slides the curve by whole octaves", "[shaping]")
{
    CurveArray in {}, out {};
    in.fill (0.0f);

    const int centre = int (LogGrid::hzToIndex (1000.0f));
    in[std::size_t (centre)] = 10.0f;

    shaping::applyFreqShift (in, out, 12.0f);   // one octave up

    const int expected = int (LogGrid::hzToIndex (2000.0f));
    int peak = 0;

    for (int i = 1; i < LogGrid::kSize; ++i)
        if (out[std::size_t (i)] > out[std::size_t (peak)])
            peak = i;

    REQUIRE (std::abs (peak - expected) <= 1);
}

TEST_CASE ("tilt pivots at 1 kHz", "[shaping]")
{
    CurveArray c {};
    c.fill (0.0f);
    shaping::applyTilt (c, 6.0f);   // +6 dB per decade

    const auto at = [&c] (float hz) { return c[std::size_t (LogGrid::hzToIndex (hz))]; };

    REQUIRE_THAT (double (at (1000.0f)), WithinAbs (0.0, 0.05));
    REQUIRE_THAT (double (at (10000.0f)), WithinAbs (6.0, 0.05));
    REQUIRE_THAT (double (at (100.0f)), WithinAbs (-6.0, 0.05));
}

TEST_CASE ("gaussian smoothing preserves level and removes detail", "[shaping]")
{
    CurveArray in {}, out {};
    in.fill (3.0f);
    shaping::applyGaussianSmoothing (in, out, 1.0f);

    for (int i = 0; i < LogGrid::kSize; ++i)
        REQUIRE_THAT (double (out[std::size_t (i)]), WithinAbs (3.0, 1.0e-3));

    in.fill (0.0f);
    in[512] = 20.0f;
    shaping::applyGaussianSmoothing (in, out, 0.5f);

    REQUIRE (out[512] < 5.0f);          // the spike is spread...
    REQUIRE (out[520] > 0.1f);          // ...into its neighbourhood
}
