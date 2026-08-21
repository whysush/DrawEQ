#include "BiquadCascade.h"
#include <algorithm>

namespace graphite
{

void BiquadCascade::prepare (double sampleRate, int numChannels)
{
    sr       = sampleRate;
    channels = std::clamp (numChannels, 1, kMaxChannels);

    for (int i = 0; i < kMaxBands; ++i)
    {
        smoothLogF[size_t (i)].prepare (sampleRate, 20.0f);
        smoothGain[size_t (i)].prepare (sampleRate, 20.0f);
        smoothLogQ[size_t (i)].prepare (sampleRate, 20.0f);
    }

    reset();
}

void BiquadCascade::reset()
{
    for (auto& ch : state)
        for (auto& s : ch)
            s = {};

    snapToTargets();
}

void BiquadCascade::setTargets (const Band* bands, int n) noexcept
{
    numActive = std::clamp (n, 0, kMaxBands);

    for (int i = 0; i < numActive; ++i)
        targets[size_t (i)] = bands[i];

    // Slots beyond the new count keep their frequency and Q but ramp to unity,
    // so a band that disappears fades out instead of clicking off.
    for (int i = numActive; i < kMaxBands; ++i)
        targets[size_t (i)].gainDb = 0.0f;
}

void BiquadCascade::snapToTargets() noexcept
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& t = targets[size_t (i)];
        smoothLogF[size_t (i)].snap (std::log (std::max (t.freqHz, 1.0f)));
        smoothGain[size_t (i)].snap (t.gainDb);
        smoothLogQ[size_t (i)].snap (std::log (std::max (t.q, 0.01f)));
        smoothed[size_t (i)] = t;
    }

    updateCoefficients (1);
}

void BiquadCascade::updateCoefficients (int numSamples) noexcept
{
    const float nyquist = float (sr) * 0.495f;

    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& t = targets[size_t (i)];
        auto& s = smoothed[size_t (i)];

        s.type   = t.type;
        s.id     = t.id;
        s.active = t.active;
        s.freqHz = std::clamp (std::exp (smoothLogF[size_t (i)].advance (
                                   std::log (std::clamp (t.freqHz, 10.0f, nyquist)), numSamples)),
                               10.0f, nyquist);
        s.gainDb = smoothGain[size_t (i)].advance (t.gainDb, numSamples);
        s.q      = std::clamp (std::exp (smoothLogQ[size_t (i)].advance (
                                   std::log (std::clamp (t.q, 0.1f, 18.0f)), numSamples)),
                               0.1f, 18.0f);

        auto& c = coeffs[size_t (i)];

        // A section whose smoothed gain has reached zero contributes nothing;
        // skipping it is free and keeps 24-band mode cheap when most bands are
        // idle. The threshold is well below anything audible.
        if (std::abs (s.gainDb) < 1.0e-4f)
        {
            c.bypass = true;
            continue;
        }

        c.bypass = false;

        const float A = std::pow (10.0f, s.gainDb * 0.025f);
        float g = std::tan (3.14159265358979f * s.freqHz / float (sr));
        float k = 1.0f / s.q;

        switch (s.type)
        {
            case BandType::bell:
                k    = 1.0f / (s.q * A);
                c.m0 = 1.0f;
                c.m1 = k * (A * A - 1.0f);
                c.m2 = 0.0f;
                break;

            case BandType::lowShelf:
                g   /= std::sqrt (A);
                c.m0 = 1.0f;
                c.m1 = k * (A - 1.0f);
                c.m2 = A * A - 1.0f;
                break;

            case BandType::highShelf:
                g   *= std::sqrt (A);
                c.m0 = A * A;
                c.m1 = k * (1.0f - A) * A;
                c.m2 = 1.0f - A * A;
                break;
        }

        c.a1 = 1.0f / (1.0f + g * (g + k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
    }
}

void BiquadCascade::process (float* const* chans, int numChannels, int numSamples) noexcept
{
    updateCoefficients (numSamples);

    const int nch = std::min (numChannels, channels);

    for (int b = 0; b < kMaxBands; ++b)
    {
        const auto& c = coeffs[size_t (b)];

        if (c.bypass)
            continue;

        for (int ch = 0; ch < nch; ++ch)
        {
            auto& st = state[size_t (ch)][size_t (b)];
            float ic1 = st.ic1, ic2 = st.ic2;
            float* x = chans[ch];

            for (int n = 0; n < numSamples; ++n)
            {
                const float v0 = x[n];
                const float v3 = v0 - ic2;
                const float v1 = c.a1 * ic1 + c.a2 * v3;
                const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;

                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;

                x[n] = c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
            }

            st.ic1 = ic1;
            st.ic2 = ic2;
        }
    }
}

float BiquadCascade::magnitudeDb (float hz) const noexcept
{
    return response::cascadeDb (smoothed.data(), kMaxBands, hz, sr);
}

} // namespace graphite
