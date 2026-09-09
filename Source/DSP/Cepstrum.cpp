#include "Cepstrum.h"
#include <cmath>

namespace draweq
{

void MinimumPhase::prepare (int fftOrder)
{
    n   = 1 << fftOrder;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    a.assign (size_t (n), {});
    b.assign (size_t (n), {});
}

void MinimumPhase::computeImpulse (const float* magnitude, float* impulseOut)
{
    jassert (fft != nullptr);

    const int half = n / 2;

    // log |H|, mirrored to a full real symmetric spectrum.
    for (int k = 0; k <= half; ++k)
    {
        // Clamp before the log: a deep notch would otherwise give -inf here and
        // poison every cepstral coefficient.
        const float m = std::max (magnitude[k], 1.0e-6f);
        const float l = std::log (m);
        a[size_t (k)] = { l, 0.0f };

        if (k > 0 && k < half)
            a[size_t (n - k)] = { l, 0.0f };
    }

    // c = real (IFFT (log |H|))
    fft->perform (a.data(), b.data(), true);

    // Fold the cepstrum onto its causal half: this is the step that turns a
    // magnitude into a minimum-phase magnitude-and-phase pair.
    a[0] = { b[0].real(), 0.0f };

    for (int k = 1; k < half; ++k)
        a[size_t (k)] = { 2.0f * b[size_t (k)].real(), 0.0f };

    a[size_t (half)] = { b[size_t (half)].real(), 0.0f };

    for (int k = half + 1; k < n; ++k)
        a[size_t (k)] = {};

    // H = exp (FFT (chat))
    fft->perform (a.data(), b.data(), false);

    for (int k = 0; k < n; ++k)
    {
        const float mag = std::exp (b[size_t (k)].real());
        const float ph  = b[size_t (k)].imag();
        a[size_t (k)] = { mag * std::cos (ph), mag * std::sin (ph) };
    }

    fft->perform (a.data(), b.data(), true);

    for (int i = 0; i < n; ++i)
        impulseOut[i] = b[size_t (i)].real();
}

} // namespace draweq
