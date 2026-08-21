#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/ConvolutionEngine.h"
#include <random>
#include <vector>

using namespace graphite;
using Catch::Matchers::WithinAbs;

namespace
{
std::vector<float> directConvolution (const std::vector<float>& x, const std::vector<float>& h)
{
    std::vector<float> y (x.size(), 0.0f);

    for (std::size_t n = 0; n < x.size(); ++n)
    {
        double acc = 0.0;

        for (std::size_t k = 0; k < h.size() && k <= n; ++k)
            acc += double (h[k]) * double (x[n - k]);

        y[n] = float (acc);
    }

    return y;
}

std::vector<float> noise (std::size_t n, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
    std::vector<float> v (n);

    for (auto& s : v)
        s = dist (rng);

    return v;
}
} // namespace

TEST_CASE ("partition size follows the host block size", "[convolution]")
{
    REQUIRE (ConvolutionEngine::partitionSizeFor (16)   == 64);
    REQUIRE (ConvolutionEngine::partitionSizeFor (64)   == 64);
    REQUIRE (ConvolutionEngine::partitionSizeFor (100)  == 128);
    REQUIRE (ConvolutionEngine::partitionSizeFor (128)  == 128);
    REQUIRE (ConvolutionEngine::partitionSizeFor (512)  == 512);
    REQUIRE (ConvolutionEngine::partitionSizeFor (2048) == 512);
}

TEST_CASE ("UPOLS equals direct convolution", "[convolution]")
{
    const int irLength = 512;
    const auto ir = noise (std::size_t (irLength), 7);

    for (int block : { 16, 64, 128, 256, 512, 2048 })
    {
        const int P = ConvolutionEngine::partitionSizeFor (block);

        ConvolutionEngine engine;
        engine.prepare (irLength, P, block);

        std::vector<float> spectra (std::size_t (ConvolutionEngine::spectraFloatsFor (irLength, P)), 0.0f);
        engine.partitionIR (ir.data(), irLength, spectra.data());

        const int total = block * 24;
        const auto input = noise (std::size_t (total), 99);
        const auto expected = directConvolution (input, ir);

        std::vector<float> outA (std::size_t (total), 0.0f), outB (std::size_t (total), 0.0f);

        for (int pos = 0; pos < total; pos += block)
            engine.process (input.data() + pos, outA.data() + pos, outB.data() + pos, block,
                            spectra.data(), nullptr);

        const int latency = engine.latencySamples();
        INFO ("block " << block << " P " << P << " latency " << latency);

        REQUIRE (engine.underruns() == 0);

        // Compare past the reported latency: that offset is the contract the
        // host is told about via setLatencySamples.
        for (int n = 0; n + latency < total; ++n)
            REQUIRE_THAT (double (outA[std::size_t (n + latency)]),
                          WithinAbs (double (expected[std::size_t (n)]), 2.0e-4));
    }
}

TEST_CASE ("a unit impulse IR passes audio through untouched", "[convolution][null]")
{
    const int block = 128, irLength = 4096;
    const int P = ConvolutionEngine::partitionSizeFor (block);

    ConvolutionEngine engine;
    engine.prepare (irLength, P, block);

    std::vector<float> ir (std::size_t (irLength), 0.0f);
    ir[0] = 1.0f;

    std::vector<float> spectra (std::size_t (ConvolutionEngine::spectraFloatsFor (irLength, P)), 0.0f);
    engine.partitionIR (ir.data(), irLength, spectra.data());

    const int total = block * 40;
    const auto input = noise (std::size_t (total), 4242);
    std::vector<float> outA (std::size_t (total), 0.0f), outB (std::size_t (total), 0.0f);

    for (int pos = 0; pos < total; pos += block)
        engine.process (input.data() + pos, outA.data() + pos, outB.data() + pos, block,
                        spectra.data(), nullptr);

    double worst = 0.0;

    for (int n = 0; n + engine.latencySamples() < total; ++n)
        worst = std::max (worst, std::abs (double (outA[std::size_t (n + engine.latencySamples())])
                                         - double (input[std::size_t (n)])));

    // -120 dB is the bar from CONTEXT.md 11; single precision FFT round-trip
    // over a 4096-tap IR lands comfortably under it.
    REQUIRE (20.0 * std::log10 (std::max (worst, 1.0e-12)) < -120.0);
}

TEST_CASE ("both crossfade streams are computed from one input history", "[convolution]")
{
    const int block = 128, irLength = 256;
    const int P = ConvolutionEngine::partitionSizeFor (block);

    ConvolutionEngine engine;
    engine.prepare (irLength, P, block);

    const auto irA = noise (std::size_t (irLength), 1);
    const auto irB = noise (std::size_t (irLength), 2);

    const int floats = ConvolutionEngine::spectraFloatsFor (irLength, P);
    std::vector<float> specA (std::size_t (floats), 0.0f), specB (std::size_t (floats), 0.0f);
    engine.partitionIR (irA.data(), irLength, specA.data());
    engine.partitionIR (irB.data(), irLength, specB.data());

    const int total = block * 16;
    const auto input = noise (std::size_t (total), 55);
    const auto expectedA = directConvolution (input, irA);
    const auto expectedB = directConvolution (input, irB);

    std::vector<float> outA (std::size_t (total), 0.0f), outB (std::size_t (total), 0.0f);

    for (int pos = 0; pos < total; pos += block)
        engine.process (input.data() + pos, outA.data() + pos, outB.data() + pos, block,
                        specA.data(), specB.data());

    const int latency = engine.latencySamples();

    for (int n = 0; n + latency < total; ++n)
    {
        REQUIRE_THAT (double (outA[std::size_t (n + latency)]),
                      WithinAbs (double (expectedA[std::size_t (n)]), 2.0e-4));
        REQUIRE_THAT (double (outB[std::size_t (n + latency)]),
                      WithinAbs (double (expectedB[std::size_t (n)]), 2.0e-4));
    }
}
