#pragma once

#include <cmath>

namespace graphite
{

/**
    One-pole parameter ramp. Trivially copyable, no allocation, safe on the
    audio thread. `process` is a single multiply-add.
*/
class OnePole
{
public:
    void prepare (double sampleRate, float timeMs) noexcept
    {
        // Time constant expressed as the 1/e point, evaluated per sample.
        const double tau = double (timeMs) * 0.001 * sampleRate;
        coeff = tau > 0.0 ? float (std::exp (-1.0 / tau)) : 0.0f;
    }

    /** Per-block use: call once with the block length to advance the ramp by a
        whole block. Cheaper than per-sample and inaudible for 20 ms ramps on
        coefficients that are themselves only recomputed per block. */
    float advance (float target, int numSamples) noexcept
    {
        const float a = std::pow (coeff, float (numSamples));
        value = target + a * (value - target);
        return value;
    }

    float process (float target) noexcept
    {
        value = target + coeff * (value - target);
        return value;
    }

    void snap (float v) noexcept { value = v; }
    float get() const noexcept   { return value; }

private:
    float coeff = 0.0f;
    float value = 0.0f;
};

} // namespace graphite
