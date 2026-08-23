#include "Analyzer.h"
#include <algorithm>
#include <cmath>

namespace draweq
{

namespace
{
    constexpr float kFloorDb    = -100.0f;
    constexpr float kPeakFallDb = 40.0f;   // 60 dB in 1.5 s
}

void Analyzer::Ring::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    int w = write.load (std::memory_order_relaxed);
    const float scale = numChannels > 0 ? 1.0f / float (numChannels) : 1.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        float sum = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            sum += channels[ch][n];

        data[std::size_t (w)] = sum * scale;
        w = (w + 1) & (kRingSize - 1);
    }

    write.store (w, std::memory_order_release);
}

int Analyzer::Ring::available() const noexcept
{
    const int w = write.load (std::memory_order_acquire);
    return (w - read + kRingSize) & (kRingSize - 1);
}

bool Analyzer::Ring::popHop (float* dest, int hop) noexcept
{
    if (available() < hop)
        return false;

    for (int i = 0; i < hop; ++i)
        dest[i] = data[std::size_t ((read + i) & (kRingSize - 1))];

    read = (read + hop) & (kRingSize - 1);
    return true;
}

void Analyzer::prepare (double sampleRate)
{
    sr  = sampleRate;
    fft = std::make_unique<juce::dsp::FFT> (kFftOrder);

    for (int i = 0; i < kFftSize; ++i)
        hann[std::size_t (i)] = 0.5f * (1.0f - std::cos (6.28318530717959f * float (i) / float (kFftSize)));

    fftBuffer.assign (std::size_t (2 * kFftSize), 0.0f);
    preWindow.assign (std::size_t (kFftSize), 0.0f);
    postWindow.assign (std::size_t (kFftSize), 0.0f);

    // Log display point -> nearest linear FFT bin, precomputed once. Below a
    // few hundred Hz several display points share a bin; that is the honest
    // resolution of a 4096-point transform and smoothing it would be a lie.
    for (int i = 0; i < kPoints; ++i)
    {
        const float hz  = pointToHz (i);
        const int   bin = int (std::lround (double (hz) * kFftSize / sr));
        binForPoint[std::size_t (i)] = std::clamp (bin, 1, kFftSize / 2);
    }

    reset();
}

void Analyzer::reset()
{
    preDisplay.fill (kFloorDb);
    postDisplay.fill (kFloorDb);
    postPeak.fill (kFloorDb);
    preRing.read  = preRing.write.load (std::memory_order_relaxed);
    postRing.read = postRing.write.load (std::memory_order_relaxed);
}

void Analyzer::pushPre (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (preEnabled.load (std::memory_order_relaxed))
        preRing.push (channels, numChannels, numSamples);
}

void Analyzer::pushPost (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (postEnabled.load (std::memory_order_relaxed))
        postRing.push (channels, numChannels, numSamples);
}

void Analyzer::analyse (Ring& ring, std::vector<float>& window, std::array<float, kPoints>& display,
                        std::array<float, kPoints>* peak, float secondsSinceLast, bool enabled)
{
    if (! enabled)
    {
        for (auto& v : display)
            v = std::max (kFloorDb, v - kPeakFallDb * secondsSinceLast);

        return;
    }

    bool gotFrame = false;

    // Drain everything queued: if the UI stalled we want the newest picture,
    // not a backlog played back in slow motion.
    while (ring.available() >= kHop)
    {
        std::memmove (window.data(), window.data() + kHop,
                      sizeof (float) * std::size_t (kFftSize - kHop));

        if (! ring.popHop (window.data() + (kFftSize - kHop), kHop))
            break;

        gotFrame = true;
    }

    if (! gotFrame)
    {
        for (auto& v : display)
            v = std::max (kFloorDb, v - kPeakFallDb * secondsSinceLast * 0.5f);

        if (peak != nullptr)
            for (auto& v : *peak)
                v = std::max (kFloorDb, v - kPeakFallDb * secondsSinceLast);

        return;
    }

    std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);

    for (int i = 0; i < kFftSize; ++i)
        fftBuffer[std::size_t (i)] = window[std::size_t (i)] * hann[std::size_t (i)];

    fft->performFrequencyOnlyForwardTransform (fftBuffer.data(), true);

    // Coherent gain of a Hann window is 0.5, and the transform is unnormalised.
    const float norm = 2.0f / (float (kFftSize) * 0.5f);

    for (int i = 0; i < kPoints; ++i)
    {
        const int   bin = binForPoint[std::size_t (i)];
        const float hz  = pointToHz (i);

        // Above the point where one display step spans more than one bin, take
        // the maximum across the span: a peak that falls between display points
        // must not disappear.
        int lo = bin, hi = bin;

        if (i > 0)              lo = std::min (lo, binForPoint[std::size_t (i - 1)] + 1);
        if (i < kPoints - 1)    hi = std::max (hi, binForPoint[std::size_t (i + 1)] - 1);

        lo = std::clamp (lo, 1, kFftSize / 2);
        hi = std::clamp (std::max (hi, lo), 1, kFftSize / 2);

        float mag = 0.0f;

        for (int b = lo; b <= hi; ++b)
            mag = std::max (mag, fftBuffer[std::size_t (b)]);

        // +4.5 dB per octave so that pink noise, which is what music broadly
        // looks like, reads as a flat line rather than a slope.
        const float db = 20.0f * std::log10 (std::max (mag * norm, 1.0e-9f))
                       + 4.5f * std::log2 (std::max (hz, 20.0f) / 1000.0f);

        const float shown = std::max (kFloorDb, db);

        // Fast attack, slow release: the eye wants the peak, not the average.
        display[std::size_t (i)] = shown > display[std::size_t (i)]
                                 ? shown
                                 : std::max (shown, display[std::size_t (i)] - kPeakFallDb * secondsSinceLast);

        if (peak != nullptr)
        {
            auto& p = (*peak)[std::size_t (i)];
            p = shown > p ? shown
                          : std::max (kFloorDb, p - kPeakFallDb * secondsSinceLast * 0.67f);
        }
    }
}

void Analyzer::update (float secondsSinceLast)
{
    if (fft == nullptr)
        return;

    secondsSinceLast = std::clamp (secondsSinceLast, 0.001f, 0.25f);

    analyse (preRing,  preWindow,  preDisplay,  nullptr,   secondsSinceLast,
             preEnabled.load (std::memory_order_relaxed));
    analyse (postRing, postWindow, postDisplay, &postPeak, secondsSinceLast,
             postEnabled.load (std::memory_order_relaxed));
}

} // namespace draweq
