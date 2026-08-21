#include <catch2/catch_test_macros.hpp>

#include "DSP/BiquadCascade.h"
#include "DSP/ConvolutionEngine.h"
#include "DSP/CurveWorker.h"
#include <random>

using namespace graphite;

namespace
{
constexpr double kSr = 48000.0;
constexpr int    kBlock = 128;

std::vector<float> noise (int n, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-0.7f, 0.7f);
    std::vector<float> v (static_cast<std::size_t> (n), 0.0f);

    for (auto& s : v)
        s = dist (rng);

    return v;
}

double worstErrorDb (const std::vector<float>& out, const std::vector<float>& in, int latency)
{
    double worst = 0.0;

    // Skip the first block: the convolution's history starts empty, so the very
    // beginning is a genuine transient rather than an error.
    for (int n = kBlock; n + latency < int (in.size()); ++n)
        worst = std::max (worst, std::abs (double (out[std::size_t (n + latency)])
                                         - double (in[std::size_t (n)])));

    return 20.0 * std::log10 (std::max (worst, 1.0e-13));
}
} // namespace

TEST_CASE ("a flat curve nulls against dry audio", "[null]")
{
    // The highest-value test in the suite: a whole class of gain, phase, shift
    // and latency bugs shows up here and almost nowhere else (CONTEXT.md 11).
    CurveModel model;
    CurveWorker worker;
    worker.setSource (&model);
    worker.prepare (kSr, kBlock);
    worker.setMacros (0.0f, 0.0f, 0.0f, 12, Mode::analog);

    const int total = kBlock * 64;
    const auto input = noise (total, 31337);

    SECTION ("Analog mode")
    {
        worker.buildOnceForTesting();

        auto* state = worker.ring().consume();
        REQUIRE (state != nullptr);
        REQUIRE (state->latencySamples == 0);

        BiquadCascade cascade;
        cascade.prepare (kSr, 1);
        cascade.setTargets (state->bands.data(), state->numBands);
        cascade.snapToTargets();

        auto out = input;

        for (int pos = 0; pos < total; pos += kBlock)
        {
            float* ch[1] = { out.data() + pos };
            cascade.process (ch, 1, kBlock);
        }

        const double err = worstErrorDb (out, input, 0);
        INFO ("analog null at " << err << " dB");
        REQUIRE (err < -120.0);
    }

    SECTION ("Spectral modes")
    {
        for (Mode mode : { Mode::spectralLinear, Mode::spectralMinimum })
        {
            worker.setMacros (0.0f, 0.0f, 0.0f, 12, mode);
            worker.buildOnceForTesting();

            auto* state = worker.ring().consume();
            REQUIRE (state != nullptr);
            REQUIRE (state->mode == mode);

            ConvolutionEngine engine;
            engine.prepare (state->irLength, state->partitionSize, kBlock);

            std::vector<float> outA (std::size_t (total), 0.0f), outB (std::size_t (total), 0.0f);

            for (int pos = 0; pos < total; pos += kBlock)
                engine.process (input.data() + pos, outA.data() + pos, outB.data() + pos, kBlock,
                                state->irSpectra.data(), nullptr);

            const int latency = state->latencySamples + engine.latencySamples();
            const double err = worstErrorDb (outA, input, latency);

            INFO ((mode == Mode::spectralLinear ? "linear" : "minimum")
                  << " phase null at " << err << " dB, latency " << latency);
            REQUIRE (engine.underruns() == 0);
            REQUIRE (err < -120.0);

            worker.ring().retire (state);
            worker.ring().drainRecycle();
        }
    }
}

TEST_CASE ("the worker publishes at most one state per change", "[worker]")
{
    CurveModel model;
    CurveWorker worker;
    worker.setSource (&model);
    worker.prepare (kSr, kBlock);
    worker.setMacros (0.0f, 15.0f, 0.0f, 12, Mode::analog);

    worker.buildOnceForTesting();
    const auto first = worker.publishCount();
    REQUIRE (first == 1);

    // Nothing changed: the worker must go back to sleep rather than spin out a
    // new state every tick.
    worker.buildOnceForTesting();
    worker.buildOnceForTesting();
    REQUIRE (worker.publishCount() == first);

    model.beginGesture();
    model.startStroke (1000.0f, 0.0f);
    model.strokeTo (1000.0f, 6.0f, 0.5f, 1.0f, CurveModel::Brush::draw);
    model.endGesture();

    auto* stale = worker.ring().consume();
    worker.ring().retire (stale);

    worker.buildOnceForTesting();
    REQUIRE (worker.publishCount() == first + 1);
}

TEST_CASE ("the worker survives its pool running dry", "[worker]")
{
    CurveModel model;
    CurveWorker worker;
    worker.setSource (&model);
    worker.prepare (kSr, kBlock);
    worker.setMacros (0.0f, 0.0f, 0.0f, 12, Mode::analog);

    // Never consume: after four publishes the pool is exhausted and the worker
    // must drop frames instead of allocating or blocking.
    for (int i = 0; i < 20; ++i)
    {
        model.beginGesture();
        model.startStroke (500.0f + float (i) * 100.0f, 0.0f);
        model.strokeTo (500.0f + float (i) * 100.0f, 4.0f, 0.4f, 1.0f, CurveModel::Brush::draw);
        model.endGesture();

        worker.buildOnceForTesting();
    }

    REQUIRE (worker.publishCount() > 0);
    REQUIRE (worker.ring().consume() != nullptr);
}
