#include <catch2/catch_test_macros.hpp>

#include "DSP/BiquadCascade.h"
#include "DSP/ConvolutionEngine.h"
#include "DSP/CurveWorker.h"
#include <atomic>
#include <cstdlib>
#include <new>

using namespace draweq;

// ---------------------------------------------------------------------------
// A global allocation trap.
//
// Replacing operator new for the whole test binary is heavy-handed, and that is
// the point: it means nothing inside the guarded region can allocate without
// being counted, including code we did not think to look at. CONTEXT.md 11 asks
// for exactly this check, and it is the only way to keep the audio thread's
// rules from rotting over time (CONTEXT.md 5).
// ---------------------------------------------------------------------------

namespace
{
    std::atomic<bool> trapArmed { false };
    std::atomic<int>  allocationCount { 0 };

    struct AllocationTrap
    {
        AllocationTrap()  { allocationCount.store (0); trapArmed.store (true); }
        ~AllocationTrap() { trapArmed.store (false); }
        static int count() { return allocationCount.load(); }
    };
}

void* operator new (std::size_t size)
{
    if (trapArmed.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);

    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size) { return operator new (size); }

void operator delete (void* p) noexcept              { std::free (p); }
void operator delete[] (void* p) noexcept            { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

TEST_CASE ("the audio path does not allocate", "[realtime]")
{
    constexpr double sr = 48000.0;
    constexpr int block = 128;

    CurveModel model;
    model.beginGesture();
    model.startStroke (100.0f, 0.0f);
    model.strokeTo (2000.0f, 9.0f, 0.5f, 1.0f, CurveModel::Brush::draw);
    model.endGesture();

    CurveWorker worker;
    worker.setSource (&model);
    worker.prepare (sr, block);

    BiquadCascade cascade;
    cascade.prepare (sr, 2);

    ConvolutionEngine engineL, engineR;

    std::vector<float> l (block, 0.05f), r (block, -0.05f);
    std::vector<float> outA (block, 0.0f), outB (block, 0.0f);

    SECTION ("Analog: consume, adopt, process, retire")
    {
        worker.setMacros (0.0f, 15.0f, 0.0f, 24, Mode::analog);
        worker.buildOnceForTesting();

        {
            const AllocationTrap trap;

            for (int i = 0; i < 200; ++i)
            {
                if (auto* state = worker.ring().consume())
                {
                    cascade.setTargets (state->bands.data(), state->numBands);
                    worker.ring().retire (state);
                }

                float* ch[2] = { l.data(), r.data() };
                cascade.process (ch, 2, block);
            }

            REQUIRE (AllocationTrap::count() == 0);
        }
    }

    SECTION ("Spectral: convolution with a live crossfade")
    {
        worker.setMacros (0.0f, 15.0f, 0.0f, 12, Mode::spectralMinimum);
        worker.buildOnceForTesting();

        auto* first = worker.ring().consume();
        REQUIRE (first != nullptr);

        engineL.prepare (first->irLength, first->partitionSize, block);
        engineR.prepare (first->irLength, first->partitionSize, block);

        {
            const AllocationTrap trap;

            for (int i = 0; i < 200; ++i)
            {
                // Both IRs live: this is the expensive branch, and the one most
                // likely to have grown an accidental temporary.
                engineL.process (l.data(), outA.data(), outB.data(), block,
                                 first->irSpectra.data(), first->irSpectra.data());
                engineR.process (r.data(), outA.data(), outB.data(), block,
                                 first->irSpectra.data(), nullptr);
            }

            REQUIRE (AllocationTrap::count() == 0);
        }

        worker.ring().retire (first);
    }
}

TEST_CASE ("retiring a state never destroys it on the audio thread", "[realtime]")
{
    // The pool entries own vectors. If the audio thread ever freed one, the
    // free would show up as an allocation-adjacent call here and, in the real
    // plugin, as a dropout under load.
    CurveModel model;
    CurveWorker worker;
    worker.setSource (&model);
    worker.prepare (48000.0, 64);
    worker.setMacros (0.0f, 0.0f, 0.0f, 12, Mode::analog);
    worker.buildOnceForTesting();

    auto* state = worker.ring().consume();
    REQUIRE (state != nullptr);

    const auto* bufferBefore = state->irSpectra.data();

    {
        const AllocationTrap trap;
        worker.ring().retire (state);
        REQUIRE (AllocationTrap::count() == 0);
    }

    worker.ring().drainRecycle();

    // The entry came back intact, same storage, ready to be refilled.
    REQUIRE (state->irSpectra.data() == bufferBefore);
}
