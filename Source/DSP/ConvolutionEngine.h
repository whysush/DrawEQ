#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>

namespace draweq
{

/**
    Uniformly-partitioned overlap-save convolution.

    One engine per channel. The frequency-domain delay line holds *input*
    history, which does not depend on the impulse response - so during a
    crossfade both the outgoing and incoming IRs are convolved against the same
    FDL and only the accumulate-and-inverse-transform stage is duplicated. That
    halves the cost of an IR change and, more importantly, guarantees the two
    streams are sample-aligned.

    Non-uniform partitioning would buy nothing at these IR lengths and costs a
    great deal of complexity.
*/
class ConvolutionEngine
{
public:
    /** P = clamp (next_pow2 (hostBlockSize), 64, 512). */
    static int partitionSizeFor (int hostBlockSize);

    /** Floats needed to store one partitioned IR. Only the non-negative
        frequencies are kept: performRealOnlyInverseTransform reads no more. */
    static int spectraFloatsFor (int irLength, int partitionSize);

    void prepare (int irLength, int partitionSize, int maxBlockSize);
    void reset();

    /** Pre-transforms an IR into the compact partitioned layout. Worker thread:
        it uses the engine's FFT object, so never call it while audio is running
        against the same engine - the worker owns a separate instance for this. */
    void partitionIR (const float* ir, int irLength, float* spectraOut);

    /** Audio thread. `irB` may be null; when it is not, `outB` receives the
        response of the incoming IR against the same input history, ready for
        the caller to crossfade. */
    void process (const float* in, float* outA, float* outB, int numSamples,
                  const float* irA, const float* irB) noexcept;

    int latencySamples() const noexcept  { return primedSamples; }
    int partitionSize() const noexcept   { return P; }
    int numPartitions() const noexcept   { return numParts; }

    /** Non-zero only if the host sent block sizes that the priming could not
        cover; surfaced so a test can assert it stays at zero. */
    int underruns() const noexcept { return underrunCount.load (std::memory_order_relaxed); }

private:
    void hop (const float* irA, const float* irB) noexcept;

    int P = 0, fftSize = 0, numBins = 0, numParts = 0, primedSamples = 0;
    std::unique_ptr<juce::dsp::FFT> fft;

    std::vector<float> timeBuf;     // 2P: [previous hop | current hop]
    std::vector<float> fftScratch;  // 2 * fftSize
    std::vector<float> fdl;         // numParts * 2 * numBins, circular
    std::vector<float> accA, accB;  // 2 * fftSize accumulators
    int fdlWrite = 0;
    int accumCount = 0;

    std::vector<float> outFifoA, outFifoB;
    int fifoRead = 0, fifoWrite = 0, fifoCount = 0, fifoMask = 0;

    std::atomic<int> underrunCount { 0 };
};

} // namespace draweq
