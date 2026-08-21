#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/IRBuilder.h"
#include "DSP/BandResponse.h"

using namespace graphite;
using Catch::Matchers::WithinAbs;

namespace
{
CurveArray flatCurve (float db = 0.0f)
{
    CurveArray c {};
    c.fill (db);
    return c;
}

CurveArray curveFromBand (const Band& b, double sr)
{
    CurveArray c {};

    for (int i = 0; i < LogGrid::kSize; ++i)
        c[std::size_t (i)] = response::bandDb (b, LogGrid::indexToHz (float (i)), sr);

    return c;
}
} // namespace

TEST_CASE ("IR length scales with sample rate and stays in range", "[ir]")
{
    REQUIRE (IRBuilder::irLengthFor (44100.0) == 4096);
    REQUIRE (IRBuilder::irLengthFor (48000.0) == 4096);
    REQUIRE (IRBuilder::irLengthFor (96000.0) == 8192);
    REQUIRE (IRBuilder::irLengthFor (192000.0) == 16384);

    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        const int L = IRBuilder::irLengthFor (sr);
        REQUIRE (L >= 2048);
        REQUIRE (L <= 16384);
        REQUIRE ((L & (L - 1)) == 0);   // power of two
    }
}

TEST_CASE ("a flat curve produces a unit impulse", "[ir][null]")
{
    // The single most valuable assertion in the file: it catches gain errors,
    // window errors, normalisation errors and shift errors all at once.
    IRBuilder builder;
    builder.prepare (48000.0);

    const int L = builder.irLength();
    std::vector<float> ir (std::size_t (L), 0.0f);

    SECTION ("linear phase, centred at L/2")
    {
        const int latency = builder.build (flatCurve(), false, ir.data());
        REQUIRE (latency == L / 2);

        for (int i = 0; i < L; ++i)
            REQUIRE_THAT (double (ir[std::size_t (i)]),
                          WithinAbs (i == L / 2 ? 1.0 : 0.0, 1.0e-6));
    }

    SECTION ("minimum phase, at sample zero, no latency")
    {
        const int latency = builder.build (flatCurve(), true, ir.data());
        REQUIRE (latency == 0);

        for (int i = 0; i < L; ++i)
            REQUIRE_THAT (double (ir[std::size_t (i)]),
                          WithinAbs (i == 0 ? 1.0 : 0.0, 1.0e-6));
    }
}

TEST_CASE ("a built IR has the magnitude it was asked for", "[ir]")
{
    const double sr = 48000.0;
    const Band bell { BandType::bell, 1000.0f, 9.0f, 2.0f, 0, true };
    const auto target = curveFromBand (bell, sr);

    IRBuilder builder;
    builder.prepare (sr);

    std::vector<float> ir (std::size_t (builder.irLength()), 0.0f);
    CurveArray measured {};

    for (bool minPhase : { false, true })
    {
        builder.build (target, minPhase, ir.data());
        builder.magnitudeResponse (ir.data(), builder.irLength(), measured);

        for (int i = 0; i < LogGrid::kSize; ++i)
        {
            const float hz = LogGrid::indexToHz (float (i));

            // Below 50 Hz the finite IR length simply cannot resolve the curve;
            // that limitation is real and is why the residual ribbon exists.
            if (hz < 50.0f || hz > 19000.0f)
                continue;

            INFO ((minPhase ? "min phase at " : "linear phase at ") << hz << " Hz");
            REQUIRE_THAT (double (measured[std::size_t (i)]),
                          WithinAbs (double (target[std::size_t (i)]), 0.1));
        }
    }
}

TEST_CASE ("minimum phase really is minimum phase", "[ir]")
{
    const double sr = 48000.0;
    const auto target = curveFromBand ({ BandType::lowShelf, 200.0f, 12.0f, 0.707f, 0, true }, sr);

    IRBuilder builder;
    builder.prepare (sr);

    std::vector<float> lin (std::size_t (builder.irLength()), 0.0f);
    std::vector<float> min (std::size_t (builder.irLength()), 0.0f);

    builder.build (target, false, lin.data());
    builder.build (target, true,  min.data());

    auto energyCentroid = [] (const std::vector<float>& h)
    {
        double num = 0.0, den = 0.0;

        for (std::size_t i = 0; i < h.size(); ++i)
        {
            const double e = double (h[i]) * double (h[i]);
            num += e * double (i);
            den += e;
        }

        return num / std::max (den, 1.0e-30);
    };

    // The whole point of the cepstral path: the same magnitude, with its energy
    // as early as it can possibly be.
    REQUIRE (energyCentroid (min) < energyCentroid (lin) * 0.1);
}

TEST_CASE ("a deep notch does not produce infinities", "[ir]")
{
    // log |H| of a -inf notch is what this path blows up on if the magnitude
    // clamp is ever removed.
    CurveArray target = flatCurve();

    for (int i = 500; i < 520; ++i)
        target[std::size_t (i)] = -30.0f;

    IRBuilder builder;
    builder.prepare (48000.0);
    std::vector<float> ir (std::size_t (builder.irLength()), 0.0f);

    for (bool minPhase : { false, true })
    {
        builder.build (target, minPhase, ir.data());

        for (float v : ir)
            REQUIRE (std::isfinite (v));
    }
}
