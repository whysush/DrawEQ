#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/CurveFitter.h"
#include "Core/CurveShaping.h"
#include "Core/Shapes.h"
#include <chrono>

using namespace draweq;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double kSr = 48000.0;

CurveArray fromBands (std::initializer_list<Band> bands)
{
    CurveArray c {};

    for (int i = 0; i < LogGrid::kSize; ++i)
    {
        const float hz = LogGrid::indexToHz (float (i));
        float acc = 0.0f;

        for (const auto& b : bands)
            acc += response::bandDb (b, hz, kSr);

        c[std::size_t (i)] = acc;
    }

    return c;
}

CurveArray tiltCurve (float dbPerDecade)
{
    CurveArray c {};
    c.fill (0.0f);
    shaping::applyTilt (c, dbPerDecade);
    return c;
}

CurveArray notchCurve (float depthDb, float widthOctaves, float hz)
{
    CurveArray c {};
    c.fill (0.0f);

    const float centre = LogGrid::hzToIndex (hz);
    const float halfWidth = LogGrid::octavesToBins (widthOctaves * 0.5f);

    for (int i = 0; i < LogGrid::kSize; ++i)
        if (std::abs (float (i) - centre) < halfWidth)
            c[std::size_t (i)] = depthDb;

    return c;
}

float fitError (const CurveArray& target, int bells, bool cold = true)
{
    CurveFitter fitter;
    fitter.prepare (kSr);
    fitter.setBellCount (bells);
    return fitter.fit (target, cold).maxErrorDb;
}
} // namespace

TEST_CASE ("a flat curve fits exactly and quietly", "[fitter][null]")
{
    CurveArray flat {};
    flat.fill (0.0f);

    CurveFitter fitter;
    fitter.prepare (kSr);
    const auto& r = fitter.fit (flat, true);

    REQUIRE (r.maxErrorDb < 0.01f);
    REQUIRE_THAT (double (r.gainDb), WithinAbs (0.0, 0.01));

    // Nothing should be doing anything: a flat target that came back as two
    // large bands cancelling each other would null on paper and sound wrong
    // the moment a parameter moved.
    for (int b = 0; b < r.numBands; ++b)
        REQUIRE (std::abs (r.bands[std::size_t (b)].gainDb) < 0.5f);
}

TEST_CASE ("a constant offset is taken by the broadband trim", "[fitter]")
{
    CurveArray c {};
    c.fill (5.0f);

    CurveFitter fitter;
    fitter.prepare (kSr);
    const auto& r = fitter.fit (c, true);

    REQUIRE (r.maxErrorDb < 0.05f);
    REQUIRE_THAT (double (r.gainDb), WithinAbs (5.0, 0.2));
}

TEST_CASE ("fitter regression corpus", "[fitter]")
{
    // Recorded expectations. A change that makes any of these worse is a
    // regression, whatever it improved elsewhere.
    struct Case { const char* name; CurveArray target; int bells; float maxAllowedDb; };

    const std::vector<Case> corpus {
        { "single bell",
          fromBands ({ { BandType::bell, 1000.0f, 9.0f, 2.0f, 0, true } }), 12, 0.05f },

        { "three bells",
          fromBands ({ { BandType::bell,  90.0f,  6.0f, 1.0f, 0, true },
                       { BandType::bell, 900.0f, -8.0f, 3.0f, 1, true },
                       { BandType::bell, 6000.0f, 5.0f, 1.5f, 2, true } }), 12, 0.1f },

        { "shelf pair",
          fromBands ({ { BandType::lowShelf,  120.0f, -7.0f, 0.707f, 0, true },
                       { BandType::highShelf, 5000.0f, 6.0f, 0.707f, 1, true } }), 12, 0.2f },

        { "gentle tilt",   tiltCurve (6.0f),  12, 0.5f },
        { "steep tilt",    tiltCurve (-12.0f), 12, 0.8f },

        { "eight bells, few bands",
          fromBands ({ { BandType::bell,   40.0f,  4.0f, 2.0f, 0, true },
                       { BandType::bell,  120.0f, -5.0f, 2.0f, 1, true },
                       { BandType::bell,  380.0f,  6.0f, 2.0f, 2, true },
                       { BandType::bell, 1100.0f, -4.0f, 2.0f, 3, true },
                       { BandType::bell, 3300.0f,  5.0f, 2.0f, 4, true },
                       { BandType::bell, 9000.0f, -6.0f, 2.0f, 5, true } }), 12, 0.5f },
    };

    for (const auto& c : corpus)
    {
        const float err = fitError (c.target, c.bells);
        INFO (c.name << " -> max error " << err << " dB");
        REQUIRE (err <= c.maxAllowedDb);
    }
}

TEST_CASE ("the unfittable case is reported, not hidden", "[fitter]")
{
    // A brick-wall notch two bins wide cannot be produced by any reasonable
    // biquad cascade. The contract is not that the fitter succeeds - it is that
    // maxErrorDb says so, loudly enough for the UI to offer Spectral mode
    const float err = fitError (notchCurve (-30.0f, 0.05f, 3000.0f), 12);
    INFO ("max error " << err << " dB");
    REQUIRE (err > 3.0f);
}

TEST_CASE ("more bands never fit worse", "[fitter]")
{
    const auto target = fromBands ({ { BandType::bell,   60.0f,  7.0f, 1.5f, 0, true },
                                     { BandType::bell,  240.0f, -6.0f, 2.0f, 1, true },
                                     { BandType::bell,  800.0f,  5.0f, 4.0f, 2, true },
                                     { BandType::bell, 2400.0f, -7.0f, 3.0f, 3, true },
                                     { BandType::bell, 7000.0f,  6.0f, 1.0f, 4, true } });

    const float few  = fitError (target, 4);
    const float many = fitError (target, 16);

    INFO ("4 bands " << few << " dB, 16 bands " << many << " dB");
    REQUIRE (many <= few + 0.05f);
}

TEST_CASE ("warm start tracks a moving curve inside its budget", "[fitter][performance]")
{
    CurveFitter fitter;
    fitter.prepare (kSr);
    fitter.setBellCount (12);

    CurveArray target = fromBands ({ { BandType::bell, 1000.0f, 8.0f, 2.0f, 0, true } });
    fitter.fit (target, true);   // cold, to establish the warm start

    double worstMs = 0.0;
    float  worstErr = 0.0f;

    // 120 frames of a drag: the bell sweeps two octaves while the fitter is
    // only ever allowed five iterations per frame.
    for (int frame = 0; frame < 120; ++frame)
    {
        const float hz = 1000.0f * std::pow (2.0f, float (frame) / 60.0f);
        target = fromBands ({ { BandType::bell, hz, 8.0f, 2.0f, 0, true } });

        const auto t0 = std::chrono::steady_clock::now();
        const auto& r = fitter.fit (target, false);
        const auto t1 = std::chrono::steady_clock::now();

        worstMs  = std::max (worstMs, std::chrono::duration<double, std::milli> (t1 - t0).count());
        worstErr = std::max (worstErr, r.maxErrorDb);
    }

    INFO ("worst warm frame " << worstMs << " ms, worst error " << worstErr << " dB");
    REQUIRE (worstErr < 1.0f);
    REQUIRE (worstMs < 2.0);        // the warm-fit budget
}

TEST_CASE ("cold start stays inside its budget at full band count", "[fitter][performance]")
{
    const auto target = fromBands ({ { BandType::bell,   45.0f,  8.0f, 1.5f, 0, true },
                                     { BandType::bell,  150.0f, -7.0f, 2.5f, 1, true },
                                     { BandType::bell,  500.0f,  6.0f, 3.0f, 2, true },
                                     { BandType::bell, 1800.0f, -8.0f, 2.0f, 3, true },
                                     { BandType::bell, 5200.0f,  7.0f, 1.5f, 4, true },
                                     { BandType::bell,14000.0f, -5.0f, 1.0f, 5, true } });

    CurveFitter fitter;
    fitter.prepare (kSr);
    fitter.setBellCount (24);

    const auto t0 = std::chrono::steady_clock::now();
    const auto& r = fitter.fit (target, true);
    const auto t1 = std::chrono::steady_clock::now();

    const double ms = std::chrono::duration<double, std::milli> (t1 - t0).count();
    INFO ("cold fit " << ms << " ms, max error " << r.maxErrorDb << " dB");
    REQUIRE (ms < 50.0);            // the cold-fit budget
}

TEST_CASE ("the starting shapes are shapes the filter can actually be", "[shapes][fitter]")
{
    // Shapes.h claims that building them from bells and shelves makes them
    // reproducible by the Analog path. If that stopped being true, a user would
    // pick a preset and watch MAX ERR jump for no visible reason.
    // Bounds are set just above what is measured, not at some comfortable
    // round number - a threshold with an order of magnitude of slack would
    // never catch the regression it exists to catch.
    struct Expectation { shapes::Shape shape; float maxDb; };

    const std::vector<Expectation> expected {
        { shapes::Shape::flat,       0.001f },
        { shapes::Shape::smiley,     0.005f },
        { shapes::Shape::warmTilt,   0.005f },
        { shapes::Shape::brightTilt, 0.005f },
        { shapes::Shape::deMud,      0.005f },
        { shapes::Shape::presence,   0.005f },
        { shapes::Shape::air,        0.005f },
        // The slope-based shapes are steeper than any shelf - which is why they
        // are drawn as slopes - and still land inside a quarter of a dB.
        { shapes::Shape::vocal,      0.30f },
        { shapes::Shape::rumbleCut,  0.30f },
        { shapes::Shape::lowPass,    0.05f },
        { shapes::Shape::telephone,  0.30f },
    };

    for (const auto& e : expected)
    {
        CurveArray target;
        shapes::build (e.shape, kSr, target);

        CurveFitter fitter;
        fitter.prepare (kSr);
        fitter.setBellCount (12);

        const float err = fitter.fit (target, CurveFitter::Effort::deep).maxErrorDb;

        INFO (shapes::name (e.shape) << " -> " << err << " dB");
        REQUIRE (err <= e.maxDb);
    }
}
