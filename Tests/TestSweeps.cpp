#include <catch2/catch_test_macros.hpp>

#include "DSP/BiquadCascade.h"
#include "DSP/ConvolutionEngine.h"
#include "DSP/CurveWorker.h"
#include <random>

using namespace draweq;

namespace
{
bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;

    return true;
}

CurveArray drawnCurve()
{
    CurveModel m;
    m.beginGesture();
    m.startStroke (40.0f, 0.0f);
    m.strokeTo (150.0f, 8.0f, 0.6f, 1.0f, CurveModel::Brush::draw);
    m.strokeTo (900.0f, -10.0f, 0.6f, 1.0f, CurveModel::Brush::draw);
    m.strokeTo (5000.0f, 6.0f, 0.6f, 1.0f, CurveModel::Brush::draw);
    m.strokeTo (16000.0f, -4.0f, 0.6f, 1.0f, CurveModel::Brush::draw);
    m.endGesture();
    return m.getCurve();
}
} // namespace

TEST_CASE ("every sample rate and block size produces finite audio", "[sweep]")
{
    // CONTEXT.md 11: the full matrix, plus a prepare/release cycle in the
    // middle, because hosts change both mid-session and that is where stale
    // buffer sizes surface.
    const double rates[] { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    const int blocks[] { 16, 32, 64, 128, 512, 2048 };

    const auto curve = drawnCurve();

    for (double sr : rates)
    {
        for (int block : blocks)
        {
            CurveModel model;
            model.setCurve (curve);

            CurveWorker worker;
            worker.setSource (&model);
            worker.prepare (sr, block);

            for (Mode mode : { Mode::analog, Mode::spectralLinear, Mode::spectralMinimum })
            {
                worker.setMacros (3.0f, 20.0f, 2.0f, 12, mode);
                worker.requestColdFit();
                worker.buildOnceForTesting();

                auto* state = worker.ring().consume();
                INFO ("sr " << sr << " block " << block << " mode " << int (mode));
                REQUIRE (state != nullptr);

                std::vector<float> buffer (std::size_t (block * 8), 0.0f);
                std::mt19937 rng (12345);
                std::uniform_real_distribution<float> dist (-0.5f, 0.5f);

                for (auto& s : buffer)
                    s = dist (rng);

                if (mode == Mode::analog)
                {
                    BiquadCascade cascade;
                    cascade.prepare (sr, 1);
                    cascade.setTargets (state->bands.data(), state->numBands);
                    cascade.snapToTargets();

                    for (int pos = 0; pos + block <= int (buffer.size()); pos += block)
                    {
                        float* ch[1] = { buffer.data() + pos };
                        cascade.process (ch, 1, block);
                    }
                }
                else
                {
                    ConvolutionEngine engine;
                    engine.prepare (state->irLength, state->partitionSize, block);

                    std::vector<float> outA (buffer.size(), 0.0f), outB (buffer.size(), 0.0f);

                    for (int pos = 0; pos + block <= int (buffer.size()); pos += block)
                        engine.process (buffer.data() + pos, outA.data() + pos, outB.data() + pos,
                                        block, state->irSpectra.data(), nullptr);

                    REQUIRE (engine.underruns() == 0);
                    REQUIRE (allFinite (outA));
                    buffer = outA;
                }

                REQUIRE (allFinite (buffer));
                worker.ring().retire (state);
                worker.ring().drainRecycle();
            }
        }
    }
}

TEST_CASE ("macros swept hard never produce NaN or a stuck fit", "[sweep][thrash]")
{
    // Automation thrash: every macro moving at once, every frame, for the
    // equivalent of a minute of a host writing wild automation.
    CurveModel model;
    model.setCurve (drawnCurve());

    CurveWorker worker;
    worker.setSource (&model);
    worker.prepare (48000.0, 128);

    BiquadCascade cascade;
    cascade.prepare (48000.0, 2);

    std::vector<float> l (128, 0.1f), r (128, -0.1f);
    float trimGain = 1.0f;
    float worstPeak = 0.0f;

    for (int frame = 0; frame < 600; ++frame)
    {
        const float t = float (frame) / 600.0f;

        worker.setMacros (12.0f * std::sin (t * 41.0f),
                          50.0f + 50.0f * std::sin (t * 27.0f),
                          24.0f * std::sin (t * 13.0f),
                          4 + (frame % 21),
                          Mode::analog);
        worker.setMorphAmount (0.5f + 0.5f * std::sin (t * 31.0f));
        worker.buildOnceForTesting();

        if (auto* state = worker.ring().consume())
        {
            cascade.setTargets (state->bands.data(), state->numBands);

            // The cascade on its own carries the shape; the trim carries the
            // level. Measuring one without the other would let a legitimate
            // -20 dB fit look like a 20 dB runaway, which is the processor's
            // signal path misread rather than a filter fault.
            trimGain = std::pow (10.0f, state->trimDb / 20.0f);
            worker.ring().retire (state);
            worker.ring().drainRecycle();
        }

        // Fresh input every block. Processing is in place, so reusing the
        // buffer without refilling it would feed the filter its own output and
        // measure a feedback loop rather than the filter.
        for (int i = 0; i < 128; ++i)
        {
            const float phase = float (frame * 128 + i) * 0.05f;
            l[std::size_t (i)] =  0.1f * std::sin (phase);
            r[std::size_t (i)] = -0.1f * std::sin (phase * 1.37f);
        }

        float* ch[2] = { l.data(), r.data() };
        cascade.process (ch, 2, 128);

        for (int i = 0; i < 128; ++i)
        {
            l[std::size_t (i)] *= trimGain;
            r[std::size_t (i)] *= trimGain;
        }

        REQUIRE (allFinite (l));
        REQUIRE (allFinite (r));

        for (float v : l)
            worstPeak = std::max (worstPeak, std::abs (v));
    }

    // The curve clamps at +/-30 dB and the input peaks at 0.1, so a settled
    // filter cannot exceed 3.2 here. Measured worst case under this deliberately
    // hostile modulation is about 0.25, so the bound leaves an order of
    // magnitude of headroom while still catching a genuine runaway.
    INFO ("worst transient " << worstPeak);
    REQUIRE (worstPeak < 4.0f);

    // With the macros held still, the filter must settle: an SVF that has been
    // pushed around this hard and is still ringing at the end was never stable.
    worker.setMacros (0.0f, 15.0f, 0.0f, 12, Mode::analog);
    worker.setMorphAmount (0.0f);
    worker.buildOnceForTesting();

    if (auto* state = worker.ring().consume())
    {
        cascade.setTargets (state->bands.data(), state->numBands);
        worker.ring().retire (state);
    }

    float peak = 0.0f;

    for (int block = 0; block < 200; ++block)
    {
        std::fill (l.begin(), l.end(), 0.0f);   // silence in
        std::fill (r.begin(), r.end(), 0.0f);

        float* ch[2] = { l.data(), r.data() };
        cascade.process (ch, 2, 128);

        if (block >= 190)
            for (float v : l)
                peak = std::max (peak, std::abs (v));
    }

    REQUIRE (peak < 1.0e-4f);
}

TEST_CASE ("repeated prepare and release cycles are safe", "[sweep]")
{
    CurveModel model;
    CurveWorker worker;
    worker.setSource (&model);

    for (int i = 0; i < 8; ++i)
    {
        worker.prepare (i % 2 == 0 ? 44100.0 : 96000.0, i % 2 == 0 ? 64 : 1024);
        worker.setMacros (0.0f, 15.0f, 0.0f, 8 + i, Mode::analog);
        worker.start();
        worker.buildOnceForTesting();
        worker.stop();
    }

    REQUIRE (worker.publishCount() > 0);
}
