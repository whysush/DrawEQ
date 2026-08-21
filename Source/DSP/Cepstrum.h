#pragma once

#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <vector>

namespace graphite
{

/**
    Real-cepstrum minimum-phase reconstruction.

    Given a magnitude spectrum, produces the causal impulse response with that
    magnitude and the least possible group delay. The trick is that the real
    cepstrum of a minimum-phase signal is causal, so folding the cepstrum onto
    its causal half and transforming back reconstructs the phase that the
    magnitude implies.

    Worker thread only - it allocates in prepare() and runs several transforms.
*/
class MinimumPhase
{
public:
    /** fftOrder gives N = 2^order, the oversampled work size (N = 2L). */
    void prepare (int fftOrder);

    /** `magnitude` holds N/2 + 1 linear magnitudes; `impulseOut` receives N real
        samples. Magnitudes are floored internally: log of a -inf notch is what
        this whole path is most likely to blow up on. */
    void computeImpulse (const float* magnitude, float* impulseOut);

    int size() const noexcept { return n; }

private:
    int n = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<std::complex<float>> a, b;
};

} // namespace graphite
