#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/BiquadCascade.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

using namespace draweq;
using Catch::Matchers::WithinAbs;

namespace
{
/** Measures what the filter actually does, by running an impulse through it. */
std::vector<float> measuredResponseDb (const std::vector<Band>& bands, double sr,
                                       const std::vector<float>& atHz)
{
    constexpr int order = 15, n = 1 << order;

    BiquadCascade cascade;
    cascade.prepare (sr, 1);
    cascade.setTargets (bands.data(), int (bands.size()));
    cascade.snapToTargets();

    std::vector<float> buf (std::size_t (n), 0.0f);
    buf[0] = 1.0f;

    float* channels[1] = { buf.data() };
    cascade.process (channels, 1, n);

    juce::dsp::FFT fft (order);
    std::vector<float> spectrum (std::size_t (2 * n), 0.0f);
    std::copy (buf.begin(), buf.end(), spectrum.begin());
    fft.performFrequencyOnlyForwardTransform (spectrum.data(), true);

    std::vector<float> out;
    out.reserve (atHz.size());

    for (float hz : atHz)
    {
        const int bin = int (std::lround (double (hz) * n / sr));
        out.push_back (20.0f * std::log10 (std::max (spectrum[std::size_t (bin)], 1.0e-12f)));
    }

    return out;
}
} // namespace

TEST_CASE ("the closed form is the filter, not an approximation of it", "[cascade]")
{
    // The fitter optimises against response::bandDb and the UI plots it. If it
    // drifted from what BiquadCascade builds, every fit would be solving the
    // wrong problem and the yellow line would be a polite fiction.
    const double sr = 48000.0;

    const std::vector<Band> bands {
        { BandType::bell,      120.0f,  8.0f, 1.2f,  0, true },
        { BandType::bell,     1000.0f, -9.0f, 4.0f,  1, true },
        { BandType::bell,     7500.0f,  4.5f, 0.7f,  2, true },
        { BandType::lowShelf,   90.0f, -6.0f, 0.707f, 3, true },
        { BandType::highShelf, 6000.0f, 5.0f, 0.707f, 4, true },
    };

    const std::vector<float> probes {
        30.0f, 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f,
        4000.0f, 7500.0f, 12000.0f, 18000.0f
    };

    const auto measured = measuredResponseDb (bands, sr, probes);

    for (std::size_t i = 0; i < probes.size(); ++i)
    {
        const float predicted = response::cascadeDb (bands.data(), int (bands.size()), probes[i], sr);
        INFO ("at " << probes[i] << " Hz");
        REQUIRE_THAT (double (measured[i]), WithinAbs (double (predicted), 0.05));
    }
}

TEST_CASE ("each band type behaves like its name", "[cascade]")
{
    const double sr = 48000.0;

    SECTION ("bell is local")
    {
        const std::vector<Band> b { { BandType::bell, 1000.0f, 12.0f, 3.0f, 0, true } };
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 1000.0f, sr)), WithinAbs (12.0, 0.01));
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 50.0f, sr)),   WithinAbs (0.0, 0.2));
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 16000.0f, sr)), WithinAbs (0.0, 0.5));
    }

    SECTION ("low shelf reaches its gain at DC and unity at the top")
    {
        const std::vector<Band> b { { BandType::lowShelf, 200.0f, -10.0f, 0.707f, 0, true } };
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 20.0f, sr)),    WithinAbs (-10.0, 0.4));
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 200.0f, sr)),   WithinAbs (-5.0, 0.3));
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 15000.0f, sr)), WithinAbs (0.0, 0.2));
    }

    SECTION ("high shelf is the mirror")
    {
        const std::vector<Band> b { { BandType::highShelf, 4000.0f, 8.0f, 0.707f, 0, true } };
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 20.0f, sr)),    WithinAbs (0.0, 0.2));
        REQUIRE_THAT (double (response::cascadeDb (b.data(), 1, 4000.0f, sr)),  WithinAbs (4.0, 0.3));
        REQUIRE (response::cascadeDb (b.data(), 1, 20000.0f, sr) > 7.0f);
    }
}

TEST_CASE ("the analytic bell gradient matches finite differences", "[cascade][fitter]")
{
    // This is the derivative the fitter relies on. A sign error here shows up
    // as a fit that mysteriously refuses to converge, which is a miserable bug
    // to find from the outside.
    const double sr = 48000.0;
    Band b { BandType::bell, 900.0f, 7.0f, 2.5f, 0, true };

    for (float hz : { 40.0f, 300.0f, 900.0f, 2500.0f, 14000.0f })
    {
        float df = 0.0f, dg = 0.0f, dq = 0.0f;
        response::bellGradient (b, hz, sr, df, dg, dq);

        // The step has to be large enough that the difference of two single
        // precision dB values is not swamped by cancellation - at h = 1e-4 the
        // reference is noisier than the thing it is checking.
        constexpr float h = 1.0e-2f;

        Band a1 = b, a2 = b;
        a1.freqHz = b.freqHz * std::exp (h);
        a2.freqHz = b.freqHz * std::exp (-h);
        const float fdF = (response::bandDb (a1, hz, sr) - response::bandDb (a2, hz, sr)) / (2 * h);

        a1 = b; a2 = b;
        a1.gainDb = b.gainDb + h;
        a2.gainDb = b.gainDb - h;
        const float fdG = (response::bandDb (a1, hz, sr) - response::bandDb (a2, hz, sr)) / (2 * h);

        a1 = b; a2 = b;
        a1.q = b.q * std::exp (h);
        a2.q = b.q * std::exp (-h);
        const float fdQ = (response::bandDb (a1, hz, sr) - response::bandDb (a2, hz, sr)) / (2 * h);

        INFO ("at " << hz << " Hz");
        REQUIRE_THAT (double (df), WithinAbs (double (fdF), 5.0e-3));
        REQUIRE_THAT (double (dg), WithinAbs (double (fdG), 5.0e-3));
        REQUIRE_THAT (double (dq), WithinAbs (double (fdQ), 5.0e-3));
    }
}

TEST_CASE ("a cascade with no gain is transparent", "[cascade][null]")
{
    BiquadCascade cascade;
    cascade.prepare (48000.0, 2);

    std::vector<Band> bands (kMaxBands);

    for (int i = 0; i < kMaxBands; ++i)
        bands[std::size_t (i)] = { BandType::bell, 100.0f * float (i + 1), 0.0f, 1.0f, i, true };

    cascade.setTargets (bands.data(), kMaxBands);
    cascade.snapToTargets();

    std::vector<float> l (512), r (512), refL (512);

    for (int i = 0; i < 512; ++i)
        refL[std::size_t (i)] = l[std::size_t (i)] = r[std::size_t (i)] = std::sin (float (i) * 0.1f);

    float* ch[2] = { l.data(), r.data() };
    cascade.process (ch, 2, 512);

    for (int i = 0; i < 512; ++i)
        REQUIRE_THAT (double (l[std::size_t (i)]), WithinAbs (double (refL[std::size_t (i)]), 1.0e-7));
}

TEST_CASE ("the fitter's fast path agrees with the reference form", "[cascade][fitter]")
{
    // BandEval exists purely for speed. If it ever disagreed with bandDb, the
    // fitter would be optimising a filter that does not exist.
    const double sr = 48000.0;

    for (const Band& b : { Band { BandType::bell,      250.0f, -11.0f, 5.0f,   0, true },
                           Band { BandType::bell,     3000.0f,   7.0f, 0.4f,   1, true },
                           Band { BandType::lowShelf,   80.0f,   9.0f, 0.707f, 2, true },
                           Band { BandType::highShelf, 9000.0f, -6.0f, 1.2f,   3, true } })
    {
        const auto eval = response::BandEval::make (b, sr);

        for (int i = 0; i < LogGrid::kSize; i += 7)
        {
            const float hz = LogGrid::indexToHz (float (i));
            const float tanF = response::prewarp (hz, sr);
            INFO ("band at " << b.freqHz << " Hz, probe " << hz << " Hz");
            REQUIRE_THAT (double (eval.db (tanF)),
                          WithinAbs (double (response::bandDb (b, hz, sr)), 1.0e-3));
        }
    }
}
