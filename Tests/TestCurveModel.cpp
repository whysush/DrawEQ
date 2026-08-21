#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Core/CurveModel.h"

using namespace graphite;
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
