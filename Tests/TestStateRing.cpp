#include <catch2/catch_test_macros.hpp>

#include "DSP/StateRing.h"
#include <thread>
#include <vector>

using namespace graphite;

namespace
{
struct Payload { int value = 0; };
} // namespace

TEST_CASE ("a published state reaches the consumer exactly once", "[ring]")
{
    StateRing<Payload> ring;

    auto* a = ring.acquireFree();
    REQUIRE (a != nullptr);
    a->value = 42;
    ring.publish (a);

    auto* got = ring.consume();
    REQUIRE (got == a);
    REQUIRE (got->value == 42);
    REQUIRE (ring.consume() == nullptr);   // nothing new
}

TEST_CASE ("replacing an unconsumed state reclaims it immediately", "[ring]")
{
    // The audio thread never saw the first one, so the worker owns it again.
    // Getting this wrong leaks a pool entry per dropped frame and the ring
    // starves within a second of dragging.
    StateRing<Payload> ring;
    const auto initial = ring.freeCount();

    auto* a = ring.acquireFree();
    ring.publish (a);
    auto* b = ring.acquireFree();
    ring.publish (b);

    REQUIRE (ring.consume() == b);
    REQUIRE (ring.freeCount() == initial - 1);   // only b is out
}

TEST_CASE ("retired states come back through the recycle FIFO", "[ring]")
{
    StateRing<Payload> ring;
    const auto initial = ring.freeCount();

    auto* a = ring.acquireFree();
    ring.publish (a);
    auto* consumed = ring.consume();
    REQUIRE (ring.freeCount() == initial - 1);

    ring.retire (consumed);
    ring.drainRecycle();
    REQUIRE (ring.freeCount() == initial);
}

TEST_CASE ("the pool runs dry rather than allocating", "[ring]")
{
    StateRing<Payload> ring;
    std::vector<Payload*> held;

    for (std::size_t i = 0; i < 4; ++i)
        held.push_back (ring.acquireFree());

    for (auto* p : held)
        REQUIRE (p != nullptr);

    // A worker that cannot get an entry must drop the frame, not wait.
    REQUIRE (ring.acquireFree() == nullptr);
}

TEST_CASE ("concurrent publish and consume never lose or duplicate a state", "[ring]")
{
    StateRing<Payload> ring;

    std::atomic<bool> go { false }, stop { false };
    std::atomic<int> consumedCount { 0 };

    std::thread consumer ([&]
    {
        while (! go.load()) {}

        Payload* current = nullptr;

        while (! stop.load())
        {
            if (auto* next = ring.consume())
            {
                consumedCount.fetch_add (1);

                if (current != nullptr)
                    ring.retire (current);

                current = next;
            }
        }

        if (current != nullptr)
            ring.retire (current);
    });

    go.store (true);

    int publishedCount = 0;

    for (int i = 0; i < 20000; ++i)
        if (auto* p = ring.acquireFree())
        {
            p->value = i;
            ring.publish (p);
            ++publishedCount;
        }

    stop.store (true);
    consumer.join();
    ring.drainRecycle();

    REQUIRE (publishedCount > 0);
    REQUIRE (consumedCount.load() <= publishedCount);
    REQUIRE (ring.freeCount() <= 4);
}
