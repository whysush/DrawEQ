#include "BandMatcher.h"
#include <algorithm>
#include <cmath>

namespace draweq
{

void BandMatcher::match (Band* bands, int numBells, int numBands) noexcept
{
    numBells = std::clamp (numBells, 0, kMaxBands);
    numBands = std::clamp (numBands, numBells, kMaxBands);

    if (! have || numBells == 0)
    {
        for (int i = 0; i < numBands; ++i)
        {
            bands[i].id = i;
            previous[std::size_t (i)] = bands[i];
        }

        have = numBells > 0;
        return;
    }

    // Rank both sets by frequency. Matching rank to rank is the assignment that
    // cannot move anything at all: the nth-lowest band this frame goes to the
    // slot that held the nth-lowest band last frame. Anything cleverer risks a
    // token crossing the canvas, and the only reason to depart from it is a
    // genuine adjacent swap - which is handled below, and is itself a movement
    // of exactly one position.
    std::array<int, kMaxBands> newOrder {}, prevOrder {};

    for (int i = 0; i < numBells; ++i)
    {
        newOrder[std::size_t (i)]  = i;
        prevOrder[std::size_t (i)] = i;
    }

    std::sort (newOrder.begin(), newOrder.begin() + numBells,
               [bands] (int a, int b) { return bands[a].freqHz < bands[b].freqHz; });

    std::sort (prevOrder.begin(), prevOrder.begin() + numBells,
               [this] (int a, int b)
               { return previous[std::size_t (a)].freqHz < previous[std::size_t (b)].freqHz; });

    auto logDistance = [&] (int newIndex, int slot)
    {
        return std::abs (std::log (std::max (bands[newIndex].freqHz, 1.0f))
                       - std::log (std::max (previous[std::size_t (slot)].freqHz, 1.0f)));
    };

    // Disjoint adjacent swaps where swapping is a strictly better match. A tie
    // keeps the existing assignment, which is what "ambiguous means do not
    // move" has to mean in practice.
    for (int r = 0; r + 1 < numBells; ++r)
    {
        const int nA = newOrder[std::size_t (r)],     nB = newOrder[std::size_t (r + 1)];
        const int sA = prevOrder[std::size_t (r)],    sB = prevOrder[std::size_t (r + 1)];

        const float straight = logDistance (nA, sA) + logDistance (nB, sB);
        const float crossed  = logDistance (nA, sB) + logDistance (nB, sA);

        if (crossed < straight - 1.0e-4f)
        {
            std::swap (prevOrder[std::size_t (r)], prevOrder[std::size_t (r + 1)]);
            ++r;   // keep the swaps disjoint so nothing moves twice
        }
    }

    std::array<Band, kMaxBands> assigned {};

    for (int r = 0; r < numBells; ++r)
    {
        const int slot = prevOrder[std::size_t (r)];
        assigned[std::size_t (slot)] = bands[newOrder[std::size_t (r)]];
        assigned[std::size_t (slot)].id = slot;
    }

    for (int s = 0; s < numBells; ++s)
        bands[s] = assigned[std::size_t (s)];

    for (int s = numBells; s < numBands; ++s)
        bands[s].id = s;

    for (int i = 0; i < numBands; ++i)
        previous[std::size_t (i)] = bands[i];
}

} // namespace draweq
