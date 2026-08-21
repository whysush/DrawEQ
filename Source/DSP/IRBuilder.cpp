#include "IRBuilder.h"
#include "../Core/LogGrid.h"
#include <algorithm>
#include <cmath>

namespace graphite
{

static int nextPow2 (int v)
{
    int p = 1;

    while (p < v)
        p <<= 1;

    return p;
}

int IRBuilder::irLengthFor (double sampleRate)
{
    // ~85 ms of impulse response: long enough for usable resolution at 30 Hz,
    // short enough that the linear-phase latency stays under half a bar at any
    // sane tempo.
    return std::clamp (nextPow2 (int (0.085 * sampleRate)), 2048, 16384);
}

void IRBuilder::prepare (double sampleRate)
{
    sr = sampleRate;
    L  = irLengthFor (sampleRate);
    N  = 2 * L;

    order = 0;

    while ((1 << order) < N)
        ++order;

    fft = std::make_unique<juce::dsp::FFT> (order);
    minPhase.prepare (order);

    magnitude.assign (size_t (N / 2 + 1), 1.0f);
    spec.assign (size_t (N), {});
    time.assign (size_t (N), {});
    scratch.assign (size_t (N), 0.0f);
}

void IRBuilder::buildMagnitude (const CurveArray& curveDb)
{
    const int   half   = N / 2;
    const float binHz  = float (sr) / float (N);

    // Above 0.45 * SR the curve is held flat. At 44.1 kHz this does nothing
    // (0.45 * SR is 19.8 kHz, essentially kFMax); it exists so that a session
    // at an unusual rate cannot ask for gain at a frequency the filter cannot
    // represent.
    const float fTop = std::min (LogGrid::kFMax, float (sr) * 0.45f);

    for (int k = 0; k <= half; ++k)
    {
        const float hz = std::clamp (float (k) * binHz, LogGrid::kFMin, fTop);

        const float idx = LogGrid::clampIndex (LogGrid::hzToIndex (hz));
        const int   i0  = int (idx);
        const int   i1  = std::min (i0 + 1, LogGrid::kSize - 1);
        const float db  = curveDb[size_t (i0)]
                        + (idx - float (i0)) * (curveDb[size_t (i1)] - curveDb[size_t (i0)]);

        magnitude[size_t (k)] = std::max (std::pow (10.0f, db * 0.05f), 1.0e-6f);
    }
}

int IRBuilder::build (const CurveArray& curveDb, bool minimumPhase, float* irOut)
{
    jassert (fft != nullptr);

    buildMagnitude (curveDb);

    if (minimumPhase)
    {
        minPhase.computeImpulse (magnitude.data(), scratch.data());

        std::copy (scratch.begin(), scratch.begin() + L, irOut);

        // Half-Hann over the last quarter. A hard truncation of a minimum-phase
        // tail rings; fading it costs a little resolution at the very bottom
        // and removes the ringing entirely.
        const int fadeStart = (L * 3) / 4;
        const int fadeLen   = L - fadeStart;

        for (int i = 0; i < fadeLen; ++i)
        {
            const float t = float (i) / float (fadeLen);
            irOut[fadeStart + i] *= 0.5f * (1.0f + std::cos (3.14159265358979f * t));
        }

        return 0;
    }

    // Linear phase: zero phase spectrum, symmetric impulse, centred at L/2.
    const int half = N / 2;

    for (int k = 0; k <= half; ++k)
    {
        spec[size_t (k)] = { magnitude[size_t (k)], 0.0f };

        if (k > 0 && k < half)
            spec[size_t (N - k)] = { magnitude[size_t (k)], 0.0f };
    }

    fft->perform (spec.data(), time.data(), true);

    // Circular shift by L/2 and take L samples: the zero-phase response is
    // symmetric about sample 0, so its energy lives at both ends of the buffer.
    for (int i = 0; i < L; ++i)
    {
        const int src = (i - L / 2 + N) % N;
        irOut[i] = time[size_t (src)].real();
    }

    // Periodic Hann of length L. w[L/2] is exactly 1, which is what makes a
    // flat 0 dB curve come out as an exact unit impulse (tested).
    for (int i = 0; i < L; ++i)
        irOut[i] *= 0.5f * (1.0f - std::cos (6.28318530717959f * float (i) / float (L)));

    return L / 2;
}

void IRBuilder::magnitudeResponse (const float* ir, int length, CurveArray& outDb)
{
    std::fill (spec.begin(), spec.end(), std::complex<float> {});

    for (int i = 0; i < std::min (length, N); ++i)
        spec[size_t (i)] = { ir[i], 0.0f };

    fft->perform (spec.data(), time.data(), false);

    const int   half  = N / 2;
    const float binHz = float (sr) / float (N);

    for (int g = 0; g < LogGrid::kSize; ++g)
    {
        const float hz  = LogGrid::indexToHz (float (g));
        const float pos = std::clamp (hz / binHz, 0.0f, float (half));
        const int   k0  = int (pos);
        const int   k1  = std::min (k0 + 1, half);
        const float t   = pos - float (k0);

        const float m0 = std::abs (time[size_t (k0)]);
        const float m1 = std::abs (time[size_t (k1)]);

        // Interpolating magnitudes rather than dB keeps a deep notch from
        // reading as a wall of -120 dB between two bins.
        outDb[size_t (g)] = 20.0f * std::log10 (std::max (m0 + t * (m1 - m0), 1.0e-9f));
    }
}

} // namespace graphite
