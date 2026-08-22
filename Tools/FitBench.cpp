/**
    Offline fitter benchmark.

    Prints fit error and wall-clock time for a corpus of target curves, cold and
    warm, at every band count that matters. Use it to answer "did that change to
    the fitter actually help", which the unit tests deliberately cannot: they
    assert thresholds, and a threshold tells you nothing about the margin.

        GraphiteFitBench [bandCount]
*/

#include "../Source/Core/CurveShaping.h"
#include "../Source/DSP/CurveFitter.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace graphite;

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

CurveArray tilted (float dbPerDecade)
{
    CurveArray c {};
    c.fill (0.0f);
    shaping::applyTilt (c, dbPerDecade);
    return c;
}

CurveArray notch (float depth, float widthOct, float hz)
{
    CurveArray c {};
    c.fill (0.0f);

    const float centre = LogGrid::hzToIndex (hz);
    const float half   = LogGrid::octavesToBins (widthOct * 0.5f);

    for (int i = 0; i < LogGrid::kSize; ++i)
        if (std::abs (float (i) - centre) < half)
            c[std::size_t (i)] = depth;

    return c;
}

struct Case { std::string name; CurveArray target; };
}

int main (int argc, char** argv)
{
    const int bandCount = argc > 1 ? std::atoi (argv[1]) : 12;

    const std::vector<Case> corpus {
        { "flat",          CurveArray {} },
        { "single bell",   fromBands ({ { BandType::bell, 1000.0f, 9.0f, 2.0f, 0, true } }) },
        { "three bells",   fromBands ({ { BandType::bell,   90.0f,  6.0f, 1.0f, 0, true },
                                        { BandType::bell,  900.0f, -8.0f, 3.0f, 1, true },
                                        { BandType::bell, 6000.0f,  5.0f, 1.5f, 2, true } }) },
        { "shelf pair",    fromBands ({ { BandType::lowShelf,  120.0f, -7.0f, 0.707f, 0, true },
                                        { BandType::highShelf, 5000.0f, 6.0f, 0.707f, 1, true } }) },
        { "gentle tilt",   tilted (6.0f) },
        { "steep tilt",    tilted (-12.0f) },
        { "narrow notch",  notch (-30.0f, 0.05f, 3000.0f) },
        { "wide scoop",    notch (-12.0f, 2.0f, 800.0f) },
    };

    std::printf ("%-14s  %8s  %8s  %8s  %9s  %9s\n",
                 "case", "cold dB", "deep dB", "rms dB", "deep ms", "warm ms");
    std::printf ("%s\n", std::string (68, '-').c_str());

    for (const auto& c : corpus)
    {
        CurveFitter fitter;
        fitter.prepare (kSr);
        fitter.setBellCount (bandCount);

        const float coldErr = fitter.fit (c.target, CurveFitter::Effort::cold).maxErrorDb;

        CurveFitter deepFitter;
        deepFitter.prepare (kSr);
        deepFitter.setBellCount (bandCount);

        const auto t0 = std::chrono::steady_clock::now();
        const auto& deep = deepFitter.fit (c.target, CurveFitter::Effort::deep);
        const auto t1 = std::chrono::steady_clock::now();

        const float maxErr = deep.maxErrorDb;
        const float rmsErr = deep.rmsErrorDb;

        // Warm timing is the number that decides whether dragging feels live,
        // so measure the worst of a run rather than a single lucky frame.
        double worstWarm = 0.0;

        for (int i = 0; i < 50; ++i)
        {
            const auto w0 = std::chrono::steady_clock::now();
            fitter.fit (c.target, false);
            const auto w1 = std::chrono::steady_clock::now();
            worstWarm = std::max (worstWarm,
                                  std::chrono::duration<double, std::milli> (w1 - w0).count());
        }

        std::printf ("%-14s  %8.3f  %8.3f  %8.3f  %9.2f  %9.3f\n",
                     c.name.c_str(), coldErr, maxErr, rmsErr,
                     std::chrono::duration<double, std::milli> (t1 - t0).count(),
                     worstWarm);
    }

    std::printf ("\nwarm runs while dragging; deep runs once when a stroke is committed\n");
    return 0;
}
