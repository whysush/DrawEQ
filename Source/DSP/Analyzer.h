#pragma once

#include "../Core/LogGrid.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace draweq
{

/**
    Pre/post spectrum analyser, deliberately outside the filter path.

    The audio thread does one thing here: copy a mono sum into a lock-free ring.
    Everything expensive - windowing, the transform, the log-frequency mapping,
    peak hold - happens on the UI thread, where being late costs a dropped frame
    rather than a dropout. The whole thing can be switched off, and when it is,
    the audio thread does not even do the copy (CONTEXT.md 7.6).
*/
class Analyzer
{
public:
    static constexpr int kFftOrder = 12;                 // 4096
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kHop      = kFftSize / 4;       // 75 % overlap
    static constexpr int kPoints   = 512;                // display resolution
    static constexpr int kRingSize = 1 << 14;

    void prepare (double sampleRate);
    void reset();

    void setEnabled (bool pre, bool post) noexcept
    {
        preEnabled.store (pre, std::memory_order_relaxed);
        postEnabled.store (post, std::memory_order_relaxed);
    }

    bool anyEnabled() const noexcept
    {
        return preEnabled.load (std::memory_order_relaxed)
            || postEnabled.load (std::memory_order_relaxed);
    }

    /** Audio thread. Sums to mono and writes into the ring; no locks, no
        allocation, and a full ring simply overwrites the oldest samples. */
    void pushPre  (const float* const* channels, int numChannels, int numSamples) noexcept;
    void pushPost (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** UI thread. Consumes whatever is available and refreshes the display
        arrays. `secondsSinceLast` drives the peak-hold decay. */
    void update (float secondsSinceLast);

    const std::array<float, kPoints>& preDb()     const noexcept { return preDisplay; }
    const std::array<float, kPoints>& postDb()    const noexcept { return postDisplay; }
    const std::array<float, kPoints>& postPeakDb() const noexcept { return postPeak; }

    /** Display point index -> Hz, on the same log grid as everything else. */
    static float pointToHz (int i) noexcept
    {
        return LogGrid::normToHz (float (i) / float (kPoints - 1));
    }

private:
    struct Ring
    {
        std::array<float, kRingSize> data {};
        std::atomic<int> write { 0 };
        int read = 0;

        void push (const float* const* channels, int numChannels, int numSamples) noexcept;
        int  available() const noexcept;
        bool popHop (float* dest, int hop) noexcept;
    };

    void analyse (Ring& ring, std::vector<float>& window, std::array<float, kPoints>& display,
                  std::array<float, kPoints>* peak, float secondsSinceLast, bool enabled);

    double sr = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::array<float, kFftSize> hann {};
    std::vector<float> fftBuffer;          // 2 * kFftSize
    std::vector<float> preWindow, postWindow;
    /** Each display point is a constant-Q band, not a bin. The edges are kept
        in hertz rather than rounded to whole bins, because rounding out to a
        bin boundary over-counts a narrow band far more than a wide one - a
        13 Hz band at 1 kHz would claim four bins - and that lands as a tilt
        across the display rather than as an error you can see locally. */
    struct Band { int firstBin, lastBin; float loHz, hiHz; };
    std::array<Band, kPoints> bands {};

    /** Band energies before display smoothing. */
    std::array<float, kPoints> bandEnergy {};

    /** Display smoothing, in points either side. The points are log-spaced, so
        a fixed count is a fixed span in octaves - which is the whole point: it
        gives a tone the same visual width wherever it sits, instead of a
        hairline up top and a hump down the bottom. */
    static constexpr int kSmoothingPoints = 4;   // about a sixth of an octave

    Ring preRing, postRing;
    std::array<float, kPoints> preDisplay {}, postDisplay {}, postPeak {};

    std::atomic<bool> preEnabled { true }, postEnabled { true };
};

} // namespace draweq
