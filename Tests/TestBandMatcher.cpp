#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/BandMatcher.h"
#include "DSP/CurveFitter.h"
#include <array>
#include <numeric>

using namespace draweq;

namespace
{
/** Rank of each slot by frequency, which is what "position" means when we say
    a band may not move more than one position per frame. */
std::array<int, kMaxBands> ranksOf (const Band* bands, int n)
{
    std::array<int, kMaxBands> order {};
    std::iota (order.begin(), order.begin() + n, 0);
    std::sort (order.begin(), order.begin() + n,
               [bands] (int a, int b) { return bands[a].freqHz < bands[b].freqHz; });

    std::array<int, kMaxBands> rank {};

    for (int r = 0; r < n; ++r)
        rank[std::size_t (order[std::size_t (r)])] = r;

    return rank;
}
} // namespace

TEST_CASE ("band identity survives 500 frames of a morphing curve", "[matcher]")
{
    // If a token teleports under the user's cursor mid-drag, this is the test
    // that should have caught it.
    constexpr double sr = 48000.0;
    constexpr int bells = 10;

    CurveFitter fitter;
    fitter.prepare (sr);
    fitter.setBellCount (bells);

    BandMatcher matcher;

    std::array<int, kMaxBands> previousRanks {};
    bool havePrevious = false;
    int worstJump = 0;

    for (int frame = 0; frame < 500; ++frame)
    {
        const float t = float (frame) / 499.0f;

        CurveArray target {};

        for (int i = 0; i < LogGrid::kSize; ++i)
        {
            const float hz = LogGrid::indexToHz (float (i));
            float acc = 0.0f;

            // Five bells sweeping and swapping gains: the fit genuinely
            // reshuffles, which is exactly the case the matcher exists for.
            for (int b = 0; b < 5; ++b)
            {
                const float f0   = 60.0f * std::pow (6.0f, float (b) * 0.5f) * std::pow (2.0f, t * 0.8f);
                const float gain = 8.0f * std::sin (6.2831853f * (t + float (b) * 0.2f));
                acc += response::bandDb ({ BandType::bell, f0, gain, 1.8f, b, true }, hz, sr);
            }

            target[std::size_t (i)] = acc;
        }

        auto r = fitter.fit (target, frame == 0);
        matcher.match (r.bands.data(), bells, r.numBands);

        const auto ranks = ranksOf (r.bands.data(), bells);

        if (havePrevious)
            for (int slot = 0; slot < bells; ++slot)
                worstJump = std::max (worstJump,
                                      std::abs (ranks[std::size_t (slot)] - previousRanks[std::size_t (slot)]));

        previousRanks = ranks;
        havePrevious  = true;
    }

    INFO ("worst rank jump " << worstJump);
    REQUIRE (worstJump <= 1);
}

TEST_CASE ("the matcher keeps slots when nothing moved", "[matcher]")
{
    BandMatcher matcher;

    std::array<Band, kMaxBands> bands {};

    for (int i = 0; i < 6; ++i)
        bands[std::size_t (i)] = { BandType::bell, 100.0f * std::pow (2.0f, float (i)),
                                   3.0f, 1.0f, i, true };

    matcher.match (bands.data(), 6, 6);

    auto shuffled = bands;
    std::swap (shuffled[1], shuffled[4]);   // fitter returned the same bands, reordered

    matcher.match (shuffled.data(), 6, 6);

    for (int i = 0; i < 6; ++i)
        REQUIRE_THAT (double (shuffled[std::size_t (i)].freqHz),
                      Catch::Matchers::WithinAbs (double (bands[std::size_t (i)].freqHz), 0.0));
}
