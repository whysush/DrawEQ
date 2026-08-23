#pragma once

#include "BandResponse.h"
#include <array>

namespace draweq
{

/**
    Keeps a band's identity attached to the same perceptual band across fits.

    Without this, a re-fit that happens to return its bands in a different order
    makes every draggable token on screen teleport, and every smoother in the
    cascade glide a band from 80 Hz to 6 kHz. Both are catastrophic and neither
    is obvious from reading the fitter.

    Slots are identity: slot 3 is the same band this frame as last frame, and
    the cascade's smoother for slot 3 therefore ramps between two nearby values.
*/
class BandMatcher
{
public:
    void reset() noexcept { have = false; }

    /** Rewrites `bands` in place so that each occupies the slot of the previous
        frame's nearest band in log frequency. Shelves are left where they are:
        their slots are fixed by construction. */
    void match (Band* bands, int numBells, int numBands) noexcept;

private:
    std::array<Band, kMaxBands> previous {};
    bool have = false;
};

} // namespace draweq
