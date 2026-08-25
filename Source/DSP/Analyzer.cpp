#include "Analyzer.h"
#include <algorithm>
#include <cmath>

namespace draweq
{

namespace
{
    constexpr float kFloorDb    = -100.0f;
    constexpr float kPeakFallDb = 40.0f;   // 60 dB in 1.5 s

    /** Band energy is far below the signal's broadband level - the energy is
        spread across the spectrum, so no single band holds much of it. This
        lifts the display so that ordinary programme material sits in the upper
        half of the plate rather than along its floor. Set by measuring pink
        noise at -18 dBFS, not by taste. */
    constexpr float kDisplayOffset = 42.0f;
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

    // Each display point covers a constant-Q band, and the band's *energy* is
    // what gets displayed - not the level of one bin inside it.
    //
    // This is the difference between a spectrum plot and an RTA. A bin holds a
    // fixed slice of hertz, so a broadband signal spread over two thousand of
    // them puts a fraction of a percent of its energy in any one - which drew
    // pink noise as a faint line along the floor. Summing the band gives the
    // energy actually in that part of the spectrum, which is both the useful
    // number and the one that makes pink read flat without a corrective tilt.
    const double binHz = sr / double (kFftSize);
    const double halfStep = std::pow (double (LogGrid::kFMax / LogGrid::kFMin),
                                      0.5 / double (kPoints - 1));

    for (int i = 0; i < kPoints; ++i)
    {
        const double hz = double (pointToHz (i));
        const double lo = hz / halfStep;
        const double hi = hz * halfStep;

        auto& band = bands[std::size_t (i)];
        band.loHz = float (lo);
        band.hiHz = float (hi);

        // Bin b is centred on b * binHz and owns half a bin either side, so the
        // bins that can overlap this band run from lo - half to hi + half.
        band.firstBin = std::clamp (int (std::floor (lo / binHz - 0.5)), 1, kFftSize / 2);
        band.lastBin  = std::clamp (int (std::ceil (hi / binHz + 0.5)), band.firstBin,
                                    kFftSize / 2);
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

    const double binHzF = sr / double (kFftSize);

    std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);

    for (int i = 0; i < kFftSize; ++i)
        fftBuffer[std::size_t (i)] = window[std::size_t (i)] * hann[std::size_t (i)];

    fft->performFrequencyOnlyForwardTransform (fftBuffer.data(), true);

    // Coherent gain of a Hann window is 0.5, and the transform is unnormalised.
    const float norm = 2.0f / (float (kFftSize) * 0.5f);

    for (int i = 0; i < kPoints; ++i)
    {
        const auto& band = bands[std::size_t (i)];

        float energy = 0.0f;

        for (int b = band.firstBin; b <= band.lastBin; ++b)
        {
            // Each bin contributes the fraction of itself that lies inside the
            // band. A band narrower than a bin therefore takes a fraction of
            // one, and a wide band takes whole bins plus two partial edges -
            // the same expression covers both, with no step where the two
            // regimes meet.
            const float binLo = (float (b) - 0.5f) * float (binHzF);
            const float binHi = (float (b) + 0.5f) * float (binHzF);
            const float overlap = std::min (band.hiHz, binHi) - std::max (band.loHz, binLo);

            if (overlap <= 0.0f)
                continue;

            const float mag = fftBuffer[std::size_t (b)] * norm;
            energy += (overlap / float (binHzF)) * mag * mag;
        }

        // No corrective tilt: summing constant-Q bands already makes pink noise
        // read flat, and applying the +4.5 dB/octave of CONTEXT.md 7.6 on top
        // of it would tilt the display the other way.
        const float db = 10.0f * std::log10 (std::max (energy, 1.0e-18f)) + kDisplayOffset;

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
