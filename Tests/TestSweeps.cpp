#include <catch2/catch_test_macros.hpp>

#include "DSP/BiquadCascade.h"
#include "DSP/ConvolutionEngine.h"
#include "DSP/CurveWorker.h"
#include "DSP/TestTone.h"
#include "DSP/Analyzer.h"
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

TEST_CASE ("the audition source generates what it claims to", "[tone]")
{
    TestTone tone;
    tone.prepare (48000.0);

    juce::AudioBuffer<float> buffer (2, 512);

    auto rms = [&buffer]
    {
        double sum = 0.0;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            sum += double (buffer.getReadPointer (0)[i]) * double (buffer.getReadPointer (0)[i]);

        return std::sqrt (sum / double (buffer.getNumSamples()));
    };

    SECTION ("off leaves the buffer alone")
    {
        buffer.clear();
        tone.process (buffer, 512, TestTone::Mode::off, 1000.0f);
        REQUIRE (rms() < 1.0e-12);
    }

    SECTION ("every source produces finite audio at a sane level")
    {
        for (auto mode : { TestTone::Mode::sine, TestTone::Mode::pink, TestTone::Mode::sweep })
        {
            buffer.clear();

            // Several blocks: a sweep needs time to get anywhere, and pink
            // noise needs its filters to charge.
            for (int n = 0; n < 20; ++n)
                tone.process (buffer, 512, mode, 1000.0f);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                {
                    const float v = buffer.getReadPointer (ch)[i];
                    REQUIRE (std::isfinite (v));
                    REQUIRE (std::abs (v) <= 1.0f);
                }

            INFO ("mode " << int (mode) << " rms " << rms());
            REQUIRE (rms() > 0.01);    // audible
            REQUIRE (rms() < 0.2);     // and not hot enough to clip a boost
        }
    }

    SECTION ("the sine really is at the frequency asked for")
    {
        // Counting zero crossings is enough to catch the octave error that a
        // stray factor of two in the phase increment would produce.
        tone.prepare (48000.0);
        buffer.clear();

        constexpr float hz = 1000.0f;
        int crossings = 0;
        float previous = 0.0f;

        for (int n = 0; n < 94; ++n)     // ~1 second at 512 samples
        {
            tone.process (buffer, 512, TestTone::Mode::sine, hz);

            for (int i = 0; i < 512; ++i)
            {
                const float v = buffer.getReadPointer (0)[i];

                if (previous <= 0.0f && v > 0.0f)
                    ++crossings;

                previous = v;
            }
        }

        const double seconds = 94.0 * 512.0 / 48000.0;
        const double measured = double (crossings) / seconds;

        INFO ("measured " << measured << " Hz");
        REQUIRE (std::abs (measured - double (hz)) < 5.0);
    }
}

TEST_CASE ("the analyser reads pink noise flat, at a level worth looking at", "[analyzer]")
{
    // The two properties that make a spectrum display usable, and the two that
    // per-bin analysis gets wrong: a broadband signal must sit somewhere you
    // can see it, and pink noise must read flat rather than sloping.
    constexpr double sr = 48000.0;

    Analyzer analyzer;
    analyzer.prepare (sr);
    analyzer.setEnabled (false, true);

    TestTone tone;
    tone.prepare (sr);

    juce::AudioBuffer<float> buffer (1, 512);

    // Two seconds of pink at the audition level, pushed through as the audio
    // thread would.
    for (int n = 0; n < 190; ++n)
    {
        buffer.clear();
        tone.process (buffer, 512, TestTone::Mode::pink, 0.0f);

        const float* channels[1] = { buffer.getReadPointer (0) };
        analyzer.pushPost (channels, 1, 512);
        analyzer.update (512.0f / float (sr));
    }

    const auto& spectrum = analyzer.postDb();

    // Judged over the range the transform can actually resolve. Below a few
    // hundred Hz a 4096-point FFT has less than a band's worth of resolution,
    // and that limit is real rather than something to paper over.
    // Tilt and scatter are measured separately. A systematic slope would mean
    // the band maths is wrong; band-to-band scatter is just what one
    // realisation of noise looks like, and no amount of correct maths removes
    // it. Lumping them into one peak-to-peak number cannot tell them apart.
    float sum = 0.0f, lowSum = 0.0f, highSum = 0.0f;
    int counted = 0, lowCount = 0, highCount = 0;

    for (int i = 0; i < Analyzer::kPoints; ++i)
    {
        const float hz = Analyzer::pointToHz (i);

        if (hz < 500.0f || hz > 16000.0f)
            continue;

        sum += spectrum[std::size_t (i)];
        ++counted;

        if (hz < 2000.0f) { lowSum += spectrum[std::size_t (i)];  ++lowCount; }
        if (hz > 6000.0f) { highSum += spectrum[std::size_t (i)]; ++highCount; }
    }

    const float mean = sum / float (counted);
    const float tilt = highSum / float (highCount) - lowSum / float (lowCount);

    float scatter = 0.0f;

    for (int i = 0; i < Analyzer::kPoints; ++i)
    {
        const float hz = Analyzer::pointToHz (i);

        if (hz >= 500.0f && hz <= 16000.0f)
            scatter += (spectrum[std::size_t (i)] - mean) * (spectrum[std::size_t (i)] - mean);
    }

    scatter = std::sqrt (scatter / float (counted));

    INFO ("pink reads " << mean << " dB, tilt " << tilt << " dB across the decade, scatter "
          << scatter << " dB");

    // Flat: the whole point of summing constant-Q bands. A per-bin analyser
    // would slope by about -9 dB across this range.
    REQUIRE (std::abs (tilt) < 3.0f);
    REQUIRE (scatter < 5.0f);

    // And visible. The plate spans -24..+24 dB, so anything below about -20
    // is scraping the floor where it cannot be read against the curve.
    REQUIRE (mean > -18.0f);
    REQUIRE (mean < 12.0f);
}

TEST_CASE ("a pure tone reads as a spike, at the right place and height", "[analyzer]")
{
    // A sine is one frequency, so it must draw as a narrow spike - the width is
    // the analysis window's resolution and nothing else. What matters is that
    // the spike lands on the right frequency and that summing bands has not
    // lost the level.
    constexpr double sr = 48000.0;

    for (float hz : { 100.0f, 1000.0f, 8000.0f })
    {
        Analyzer analyzer;
        analyzer.prepare (sr);
        analyzer.setEnabled (false, true);

        TestTone tone;
        tone.prepare (sr);

        juce::AudioBuffer<float> buffer (1, 512);

        for (int n = 0; n < 190; ++n)
        {
            buffer.clear();
            tone.process (buffer, 512, TestTone::Mode::sine, hz);
            const float* channels[1] = { buffer.getReadPointer (0) };
            analyzer.pushPost (channels, 1, 512);
            analyzer.update (512.0f / float (sr));
        }

        const auto& spectrum = analyzer.postDb();

        int peak = 0;

        for (int i = 0; i < Analyzer::kPoints; ++i)
            if (spectrum[std::size_t (i)] > spectrum[std::size_t (peak)])
                peak = i;

        int within3 = 0;

        for (int i = 0; i < Analyzer::kPoints; ++i)
            if (spectrum[std::size_t (i)] > spectrum[std::size_t (peak)] - 3.0f)
                ++within3;

        // Where the spike reads is its centroid, not its first maximum.
        // Smoothing turns a one-point spike into a plateau, and taking the
        // first sample of a plateau reports its left edge - which looked like
        // a semitone of error that was not there.
        double weightSum = 0.0, weighted = 0.0;

        for (int i = 0; i < Analyzer::kPoints; ++i)
        {
            const double above = spectrum[std::size_t (i)] - (spectrum[std::size_t (peak)] - 6.0f);

            if (above <= 0.0)
                continue;

            weightSum += above;
            weighted += above * std::log2 (double (Analyzer::pointToHz (i)));
        }

        const float peakHz = float (std::exp2 (weighted / weightSum));
        const float octavesOff = std::abs (std::log2 (peakHz / hz));

        INFO (hz << " Hz -> peak " << spectrum[std::size_t (peak)] << " dB at " << peakHz
              << " Hz, -3 dB width " << within3 << " of " << Analyzer::kPoints << " points");

        REQUIRE (octavesOff < 0.09f);                        // lands where it was played
        REQUIRE (spectrum[std::size_t (peak)] > -20.0f);     // visible on a -24..+24 plate
        REQUIRE (within3 < Analyzer::kPoints / 10);          // a spike, not a smear
    }
}
