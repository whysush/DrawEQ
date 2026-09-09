#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <vector>

namespace draweq
{

/**
    Single-producer (worker) / single-consumer (audio) handoff of prepared
    state, with deferred deletion.

    The publish slot is an atomic pointer that both sides *exchange*. Because an
    exchange is a single read-modify-write, exactly one of the two threads ever
    receives a given pointer: if the worker replaces an unconsumed state it gets
    the old pointer back and can reuse it immediately, and if the audio thread
    got there first the worker's exchange returns null. There is no window in
    which both believe they own it, and no window in which the audio thread can
    observe a half-written state - the release/acquire pair orders every write
    the worker made before the exchange.

    The audio thread never allocates, never frees, never blocks and never
    destroys: retired states go back through a lock-free FIFO for the worker to
    reclaim.
*/
template <typename T, std::size_t PoolSize = 4, std::size_t FifoSize = 8>
class StateRing
{
public:
    StateRing()
    {
        for (auto& entry : pool)
            freeList.push_back (&entry);
    }

    /** Worker: every pool entry, for one-time allocation. */
    std::array<T, PoolSize>& entries() noexcept { return pool; }

    /** Worker: an entry nobody is using, or null if all four are in flight
        (which means the audio thread is behind - drop this frame rather than
        wait). */
    T* acquireFree() noexcept
    {
        drainRecycle();

        if (freeList.empty())
            return nullptr;

        T* p = freeList.back();
        freeList.pop_back();
        return p;
    }

    /** Worker: hand a filled entry to the audio thread. */
    void publish (T* state) noexcept
    {
        T* previous = slot.exchange (state, std::memory_order_release);

        // Never picked up: the audio thread has not seen it and never will, so
        // it is ours again.
        if (previous != nullptr)
            freeList.push_back (previous);
    }

    /** Audio: the newest published state, or null if nothing is new. */
    T* consume() noexcept
    {
        return slot.exchange (nullptr, std::memory_order_acquire);
    }

    /** Audio: finished with a state. Never destroys it here. */
    void retire (T* state) noexcept
    {
        if (state == nullptr)
            return;

        const std::size_t w = writeIndex.load (std::memory_order_relaxed);
        const std::size_t next = (w + 1) % FifoSize;

        // A full FIFO would mean the worker has stalled for four state changes.
        // Dropping the pointer leaks a pool entry until the next prepare, which
        // is strictly better than blocking the audio thread.
        if (next == readIndex.load (std::memory_order_acquire))
            return;

        fifo[w] = state;
        writeIndex.store (next, std::memory_order_release);
    }

    /** Worker: pull everything the audio thread has finished with. */
    void drainRecycle() noexcept
    {
        for (;;)
        {
            const std::size_t r = readIndex.load (std::memory_order_relaxed);

            if (r == writeIndex.load (std::memory_order_acquire))
                return;

            freeList.push_back (fifo[r]);
            readIndex.store ((r + 1) % FifoSize, std::memory_order_release);
        }
    }

    /** Message thread, audio stopped: back to the initial state. */
    void resetAll() noexcept
    {
        slot.store (nullptr, std::memory_order_relaxed);
        readIndex.store (0, std::memory_order_relaxed);
        writeIndex.store (0, std::memory_order_relaxed);
        freeList.clear();

        for (auto& entry : pool)
            freeList.push_back (&entry);
    }

    std::size_t freeCount() const noexcept { return freeList.size(); }

private:
    std::array<T, PoolSize> pool {};
    std::atomic<T*> slot { nullptr };

    std::array<T*, FifoSize> fifo {};
    std::atomic<std::size_t> readIndex { 0 }, writeIndex { 0 };

    std::vector<T*> freeList;   // worker thread only
};

} // namespace draweq
