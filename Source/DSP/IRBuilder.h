#pragma once

#include "../Core/CurveSnapshot.h"
#include "Cepstrum.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace graphite
{

/**
    Turns the drawn curve into an impulse response.

    Length is tied to the sample rate rather than fixed, because the thing that
    forces a long IR is resolution at the bottom of the range: at 48 kHz and
    L = 4096 the first FFT bin sits at 5.9 Hz, and anything shorter cannot
    express a 40 Hz shelf at all.

    Worker thread only.
*/
class IRBuilder
{
public:
    /** L for a given sample rate: next_pow2 (0.085 * SR), clamped. */
    static int irLengthFor (double sampleRate);

    void prepare (double sampleRate);

    /** Writes `irLength()` samples. `minimumPhase` selects the cepstral path.
        Returns the latency the IR introduces (L/2 linear, 0 minimum). */
    int build (const CurveArray& effectiveCurveDb, bool minimumPhase, float* irOut);

    int irLength() const noexcept { return L; }

    /** Magnitude response of a finished IR, resampled onto the log grid. This
        is what the plot line draws in Spectral mode: the windowing and the
        finite length do change the response slightly at the bottom of the
        range, and the residual ribbon should show that rather than pretend the
        result is exactly the target. */
    void magnitudeResponse (const float* ir, int length, CurveArray& outDb);

private:
    void buildMagnitude (const CurveArray& curveDb);

    double sr = 48000.0;
    int    L = 4096;      // IR length
    int    N = 8192;      // work size, 2L
    int    order = 13;

    std::unique_ptr<juce::dsp::FFT> fft;
    MinimumPhase minPhase;

    std::vector<float> magnitude;                 // N/2 + 1 linear magnitudes
    std::vector<std::complex<float>> spec, time;  // N complex
    std::vector<float> scratch;                   // N real
};

} // namespace graphite
