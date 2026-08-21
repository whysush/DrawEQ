#pragma once

#include "BandResponse.h"
#include "Smoothers.h"
#include <array>

namespace graphite
{

/**
    The Analog-mode filter: a cascade of topology-preserving-transform state
    variable filters (Zavalishin / Cytomic form).

    Why not direct-form biquads: a direct form II at 25 Hz and 192 kHz has
    coefficients that differ by six orders of magnitude and loses most of its
    precision, and - the reason that actually matters here - it misbehaves when
    its coefficients move every block, which is exactly what a live fit does.
    The SVF stays well conditioned at the bottom of the range and stays stable
    under continuous modulation (CONTEXT.md 7.4).

    Coefficients are recomputed per block from smoothed (f, G, Q). Raw
    coefficients are never interpolated: the path between two valid coefficient
    sets passes outside the stable region.
*/
class BiquadCascade
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Called from the audio thread when a new FilterState arrives. Copies the
        targets only - nothing is recomputed until the next block boundary. */
    void setTargets (const Band* bands, int numBands) noexcept;

    /** Jump straight to the targets, for a mode switch or a fresh start where
        a 20 ms glide would be audible as a sweep. */
    void snapToTargets() noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Magnitude of the smoothed state, for the UI plot line. Not called from
        the audio thread. */
    float magnitudeDb (float hz) const noexcept;

    int   activeSectionCount() const noexcept { return numActive; }
    const Band* currentBands() const noexcept { return smoothed.data(); }

private:
    struct Coeffs
    {
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;
        bool  bypass = true;
    };

    struct SectionState { float ic1 = 0.0f, ic2 = 0.0f; };

    void updateCoefficients (int numSamples) noexcept;

    double sr = 48000.0;
    int    channels = 2;
    int    numActive = 0;

    std::array<Band, kMaxBands>   targets {};
    std::array<Band, kMaxBands>   smoothed {};
    std::array<Coeffs, kMaxBands> coeffs {};
    std::array<std::array<SectionState, kMaxBands>, kMaxChannels> state {};

    // One ramp per slot per parameter. Slots are stable across fits (see
    // BandMatcher), so slot 3's ramp always belongs to the same band.
    std::array<OnePole, kMaxBands> smoothLogF {}, smoothGain {}, smoothLogQ {};
};

} // namespace graphite
