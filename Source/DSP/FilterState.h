#pragma once

#include "../Core/CurveSnapshot.h"
#include "BandResponse.h"
#include <array>
#include <cstdint>
#include <vector>

namespace draweq
{

/**
    Everything the audio thread needs in order to be the drawn curve, and
    nothing it does not.

    Built by the worker, handed over by pointer through StateRing, read but
    never written by the audio thread, and returned to the worker for reuse.
    All of its storage is allocated once in `allocate()` while audio is stopped:
    publishing must never allocate, and neither must retiring.

    Deliberately absent: anything the UI wants. Display data travels on its own
    path (CurveWorker::UiSnapshot) so that the lifetime of a FilterState is
    governed purely by the audio thread.
*/
struct FilterState
{
    Mode          mode          = Mode::analog;
    int           latencySamples = 0;
    std::uint64_t sourceVersion  = 0;   // CurveModel version this was built from

    // --- Analog ---
    std::array<Band, kMaxBands> bands {};
    int   numBands = 0;
    float trimDb   = 0.0f;   // broadband offset the cascade cannot express

    // --- Spectral ---
    // Partitioned, pre-transformed IR. Layout matches ConvolutionEngine's
    // compact non-negative-frequency form.
    std::vector<float> irSpectra;
    int irLength      = 0;
    int partitionSize = 0;
    int numPartitions = 0;

    /** Sizes every buffer for the worst case at this configuration. Called on
        the message thread from prepareToPlay, with audio stopped. */
    void allocate (int maxIrLength, int maxPartitionSize)
    {
        const int parts = std::max (1, (maxIrLength + maxPartitionSize - 1) / maxPartitionSize);
        irSpectra.assign (std::size_t (parts * 2 * (maxPartitionSize + 1)), 0.0f);
    }
};

} // namespace draweq
