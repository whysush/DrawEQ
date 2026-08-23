#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Core/CurveModel.h"
#include "Core/Shapes.h"
#include <string>
#include <vector>

using namespace draweq;
using Catch::Matchers::WithinAbs;

TEST_CASE ("a fresh model is flat", "[curve]")
{
    CurveModel m;

    for (float v : m.getCurve())
        REQUIRE_THAT (double (v), WithinAbs (0.0, 0.0));
}

TEST_CASE ("a stroke paints where it was drawn", "[curve]")
{
    CurveModel m;
    m.beginGesture();
    m.startStroke (1000.0f, 0.0f);

    for (int i = 0; i < 30; ++i)
        m.strokeTo (1000.0f, 9.0f, 0.5f, 1.0f, CurveModel::Brush::draw);

    m.endGesture();

    REQUIRE (m.dbAtHz (1000.0f) > 8.0f);
    REQUIRE_THAT (double (m.dbAtHz (100.0f)), WithinAbs (0.0, 0.01));
    REQUIRE_THAT (double (m.dbAtHz (10000.0f)), WithinAbs (0.0, 0.01));
}

TEST_CASE ("brush width in octaves is frequency independent", "[curve]")
{
    // Same gesture, two decades apart, must affect the same span in octaves.
    auto paintedOctaves = [] (float hz)
    {
        CurveModel m;
        m.beginGesture();
        m.startStroke (hz, 0.0f);

        for (int i = 0; i < 40; ++i)
            m.strokeTo (hz, 12.0f, 0.75f, 1.0f, CurveModel::Brush::draw);

        m.endGesture();

        const auto c = m.getCurve();
        int lo = LogGrid::kSize, hi = -1;

        for (int i = 0; i < LogGrid::kSize; ++i)
            if (c[std::size_t (i)] > 1.0f)
            {
                lo = std::min (lo, i);
                hi = std::max (hi, i);
            }

        return LogGrid::binsToOctaves (float (hi - lo));
    };

    REQUIRE_THAT (double (paintedOctaves (80.0f)),
                  WithinAbs (double (paintedOctaves (8000.0f)), 0.05));
}

TEST_CASE ("erase and smooth brushes pull the right way", "[curve]")
{
    CurveModel m;
    m.beginGesture();
    m.startStroke (500.0f, 0.0f);

    for (int i = 0; i < 40; ++i)
        m.strokeTo (500.0f, 15.0f, 0.4f, 1.0f, CurveModel::Brush::draw);

    m.endGesture();
    const float painted = m.dbAtHz (500.0f);
    REQUIRE (painted > 12.0f);

    m.beginGesture();
    m.startStroke (500.0f, 0.0f);

    for (int i = 0; i < 40; ++i)
        m.strokeTo (500.0f, 0.0f, 0.4f, 1.0f, CurveModel::Brush::erase);

    m.endGesture();
    REQUIRE (std::abs (m.dbAtHz (500.0f)) < 1.0f);
}

TEST_CASE ("a whole gesture is one undo step", "[curve]")
{
    CurveModel m;

    m.beginGesture();
    m.startStroke (200.0f, 0.0f);

    for (int i = 0; i < 50; ++i)
        m.strokeTo (200.0f + float (i) * 10.0f, 6.0f, 0.3f, 1.0f, CurveModel::Brush::draw);

    m.endGesture();

    REQUIRE (m.canUndo());
    REQUIRE (m.undo());
    REQUIRE_THAT (double (m.dbAtHz (400.0f)), WithinAbs (0.0, 1.0e-6));
    REQUIRE (m.canRedo());
    REQUIRE (m.redo());
    REQUIRE (m.dbAtHz (400.0f) > 1.0f);
}

TEST_CASE ("editing bumps the version the worker watches", "[curve]")
{
    CurveModel m;
    const auto before = m.version();

    m.beginGesture();
    m.startStroke (1000.0f, 0.0f);
    m.strokeTo (1000.0f, 3.0f, 0.5f, 1.0f, CurveModel::Brush::draw);
    m.endGesture();

    REQUIRE (m.version() > before);
}

TEST_CASE ("state round-trips through the binary format", "[curve][state]")
{
    CurveModel a;
    a.beginGesture();
    a.startStroke (60.0f, 0.0f);
    a.strokeTo (900.0f, -11.0f, 0.6f, 1.0f, CurveModel::Brush::draw);
    a.strokeTo (9000.0f, 7.0f, 0.6f, 1.0f, CurveModel::Brush::draw);
    a.endGesture();

    const auto blob = a.serialise();

    CurveModel b;
    REQUIRE (b.deserialise (blob.data(), blob.size()));

    const auto ca = a.getCurve();
    const auto cb = b.getCurve();

    for (std::size_t i = 0; i < ca.size(); ++i)
        REQUIRE_THAT (double (cb[i]), WithinAbs (double (ca[i]), 1.0e-6));
}

TEST_CASE ("corrupt state is rejected rather than loaded as noise", "[curve][state]")
{
    CurveModel m;
    auto blob = m.serialise();

    REQUIRE_FALSE (m.deserialise (nullptr, 0));
    REQUIRE_FALSE (m.deserialise (blob.data(), 4));

    blob[0] = 'X';
    REQUIRE_FALSE (m.deserialise (blob.data(), blob.size()));
}

TEST_CASE ("the curve lands where the cursor was", "[curve][accuracy]")
{
    // A drag is a sequence of mouse positions. Wherever the cursor passed, the
    // curve underneath it should read back the dB the cursor was at - that is
    // the whole contract of a drawing tool, and everything downstream (the
    // ghost, the fit, the plot) inherits any error made here.
    struct Sample { float hz, db; };
    std::vector<Sample> path;

    const int points = 40;

    for (int i = 0; i < points; ++i)
    {
        const float t = float (i) / float (points - 1);
        path.push_back ({ LogGrid::normToHz (0.15f + 0.5f * t),
                          -16.0f * t });                       // a steep ramp
    }

    CurveModel m;
    m.beginGesture();
    m.startStroke (path.front().hz, path.front().db);

    for (const auto& p : path)
        m.strokeTo (p.hz, p.db, 0.4f, 1.0f, CurveModel::Brush::draw);

    m.endGesture();

    double worst = 0.0;
    int worstIndex = -1;

    for (int i = 0; i < int (path.size()); ++i)
    {
        const double e = std::abs (double (m.dbAtHz (path[std::size_t (i)].hz))
                                 - double (path[std::size_t (i)].db));

        if (e > worst) { worst = e; worstIndex = i; }
    }

    INFO ("worst deviation from the cursor: " << worst << " dB at point " << worstIndex);

    // Measured 0.0077 dB, and the residual is interpolation into the feather
    // skirt at the two ends of the stroke rather than lag. The dab-based brush
    // this replaced scored 0.63 dB on the same path.
    REQUIRE (worst < 0.02);
}

TEST_CASE ("every starting shape is sane and distinct", "[shapes]")
{
    std::vector<CurveArray> built;

    for (int i = 0; i < int (shapes::Shape::count); ++i)
    {
        const auto shape = shapes::Shape (i);
        CurveArray c;
        shapes::build (shape, 48000.0, c);

        INFO ("shape: " << shapes::name (shape));

        for (float v : c)
        {
            REQUIRE (std::isfinite (v));
            REQUIRE (std::abs (v) <= LogGrid::kMaxDb + 1.0e-3f);
        }

        REQUIRE (std::string (shapes::name (shape)).length() > 0);
        built.push_back (c);
    }

    // Flat must be flat, and nothing else may be: a menu entry that silently
    // does nothing is worse than no entry.
    for (float v : built[0])
        REQUIRE_THAT (double (v), WithinAbs (0.0, 1.0e-6));

    for (std::size_t i = 1; i < built.size(); ++i)
    {
        float peak = 0.0f;

        for (float v : built[i])
            peak = std::max (peak, std::abs (v));

        INFO ("shape " << shapes::name (shapes::Shape (i)) << " peaks at " << peak);
        REQUIRE (peak > 1.0f);
    }
}
