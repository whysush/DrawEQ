#include "ConvolutionEngine.h"
#include <algorithm>
#include <cstring>
#include <numeric>

namespace graphite
{

static int nextPow2 (int v)
{
    int p = 1;

    while (p < v)
        p <<= 1;

    return p;
}

int ConvolutionEngine::partitionSizeFor (int hostBlockSize)
{
    return std::clamp (nextPow2 (std::max (hostBlockSize, 1)), 64, 512);
}

int ConvolutionEngine::spectraFloatsFor (int irLength, int partitionSize)
{
    const int parts = std::max (1, (irLength + partitionSize - 1) / partitionSize);
    return parts * 2 * (partitionSize + 1);
}

void ConvolutionEngine::prepare (int irLength, int partitionSize, int maxBlockSize)
{
    P        = partitionSize;
    fftSize  = 2 * P;
    numBins  = P + 1;
    numParts = std::max (1, (irLength + P - 1) / P);

    int order = 0;

    while ((1 << order) < fftSize)
        ++order;

    fft = std::make_unique<juce::dsp::FFT> (order);

    timeBuf.assign (size_t (fftSize), 0.0f);
    fftScratch.assign (size_t (2 * fftSize), 0.0f);
    fdl.assign (size_t (numParts * 2 * numBins), 0.0f);
    accA.assign (size_t (2 * fftSize), 0.0f);
    accB.assign (size_t (2 * fftSize), 0.0f);

    // The output FIFO must swallow a whole host block plus one hop.
    const int cap = nextPow2 (maxBlockSize + 2 * P);
    outFifoA.assign (size_t (cap), 0.0f);
    outFifoB.assign (size_t (cap), 0.0f);
    fifoMask = cap - 1;

    // Priming, i.e. reported latency. A hop only fires once P input samples
    // have arrived, so the output runs behind by however much input can pile up
    // between hops - which is P minus gcd (blockSize, P), not P minus blockSize:
    // those agree whenever the block size divides P (every power-of-two block,
    // which is every block a host actually sends) and the gcd form stays
    // correct when it does not.
    primedSamples = P - std::gcd (std::max (maxBlockSize, 1), P);

    reset();
}

void ConvolutionEngine::reset()
{
    std::fill (timeBuf.begin(), timeBuf.end(), 0.0f);
    std::fill (fdl.begin(), fdl.end(), 0.0f);
    std::fill (outFifoA.begin(), outFifoA.end(), 0.0f);
    std::fill (outFifoB.begin(), outFifoB.end(), 0.0f);

    fdlWrite   = 0;
    accumCount = 0;
    fifoRead   = 0;
    fifoWrite  = primedSamples;
    fifoCount  = primedSamples;
    underrunCount.store (0, std::memory_order_relaxed);
}

void ConvolutionEngine::partitionIR (const float* ir, int irLength, float* spectraOut)
{
    std::fill (spectraOut, spectraOut + numParts * 2 * numBins, 0.0f);

    for (int p = 0; p < numParts; ++p)
    {
        std::fill (fftScratch.begin(), fftScratch.end(), 0.0f);

        const int offset = p * P;
        const int count  = std::clamp (irLength - offset, 0, P);

        if (count > 0)
            std::memcpy (fftScratch.data(), ir + offset, sizeof (float) * size_t (count));

        // The second half stays zero: overlap-save needs the IR partition
        // zero-padded to twice its length, otherwise the block convolution
        // wraps and the tail folds back onto the head.
        fft->performRealOnlyForwardTransform (fftScratch.data(), true);

        std::memcpy (spectraOut + p * 2 * numBins, fftScratch.data(),
                     sizeof (float) * size_t (2 * numBins));
    }
}

void ConvolutionEngine::hop (const float* irA, const float* irB) noexcept
{
    // Forward transform of [previous P samples | current P samples].
    std::fill (fftScratch.begin(), fftScratch.end(), 0.0f);
    std::memcpy (fftScratch.data(), timeBuf.data(), sizeof (float) * size_t (fftSize));
    fft->performRealOnlyForwardTransform (fftScratch.data(), true);

    std::memcpy (fdl.data() + fdlWrite * 2 * numBins, fftScratch.data(),
                 sizeof (float) * size_t (2 * numBins));

    std::fill (accA.begin(), accA.begin() + 2 * numBins, 0.0f);

    if (irB != nullptr)
        std::fill (accB.begin(), accB.begin() + 2 * numBins, 0.0f);

    for (int p = 0; p < numParts; ++p)
    {
        const int slot = (fdlWrite - p + numParts) % numParts;
        const float* x = fdl.data() + slot * 2 * numBins;
        const float* a = irA + p * 2 * numBins;

        for (int k = 0; k < numBins; ++k)
        {
            const float xr = x[2 * k], xi = x[2 * k + 1];
            const float ar = a[2 * k], ai = a[2 * k + 1];
            accA[size_t (2 * k)]     += xr * ar - xi * ai;
            accA[size_t (2 * k + 1)] += xr * ai + xi * ar;
        }

        if (irB != nullptr)
        {
            const float* b = irB + p * 2 * numBins;

            for (int k = 0; k < numBins; ++k)
            {
                const float xr = x[2 * k], xi = x[2 * k + 1];
                const float br = b[2 * k], bi = b[2 * k + 1];
                accB[size_t (2 * k)]     += xr * br - xi * bi;
                accB[size_t (2 * k + 1)] += xr * bi + xi * br;
            }
        }
    }

    fdlWrite = (fdlWrite + 1) % numParts;

    fft->performRealOnlyInverseTransform (accA.data());

    if (irB != nullptr)
        fft->performRealOnlyInverseTransform (accB.data());

    // Overlap-save: the first P samples are circular-wrap garbage, the last P
    // are the linear convolution.
    for (int i = 0; i < P; ++i)
    {
        const int w = (fifoWrite + i) & fifoMask;
        outFifoA[size_t (w)] = accA[size_t (P + i)];
        outFifoB[size_t (w)] = irB != nullptr ? accB[size_t (P + i)] : accA[size_t (P + i)];
    }

    fifoWrite = (fifoWrite + P) & fifoMask;
    fifoCount += P;
}

void ConvolutionEngine::process (const float* in, float* outA, float* outB, int numSamples,
                                 const float* irA, const float* irB) noexcept
{
    // Push the whole block first, then drain it: draining in step with the
    // input would need P-1 samples of priming instead of P - gcd.
    int done = 0;

    while (done < numSamples)
    {
        const int want = std::min (P - accumCount, numSamples - done);

        std::memcpy (timeBuf.data() + P + accumCount, in + done, sizeof (float) * size_t (want));
        accumCount += want;
        done       += want;

        if (accumCount == P)
        {
            hop (irA, irB);
            // Slide: this hop's input becomes the next hop's history.
            std::memcpy (timeBuf.data(), timeBuf.data() + P, sizeof (float) * size_t (P));
            accumCount = 0;
        }
    }

    if (fifoCount < numSamples)
    {
        // Only reachable if the host varies its block size in a way the priming
        // could not anticipate. Emit silence for the shortfall rather than
        // reading stale samples, and count it so it cannot pass unnoticed.
        const int missing = numSamples - fifoCount;
        std::fill (outA, outA + missing, 0.0f);
        std::fill (outB, outB + missing, 0.0f);
        underrunCount.fetch_add (1, std::memory_order_relaxed);

        outA += missing;
        outB += missing;
        numSamples = fifoCount;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const int r = (fifoRead + i) & fifoMask;
        outA[i] = outFifoA[size_t (r)];
        outB[i] = outFifoB[size_t (r)];
    }

    fifoRead  = (fifoRead + numSamples) & fifoMask;
    fifoCount -= numSamples;
}

} // namespace graphite
