#pragma once

#include <atomic>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>

namespace pulseflow {

/// Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer
/// Optimized with 64-byte cache line alignment to prevent false sharing.
template <typename T, std::size_t Capacity = 65536>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    SpscRingBuffer() : buffer_(Capacity) {}
    ~SpscRingBuffer() = default;

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    /// Attempt to enqueue an item. Returns false if buffer is full.
    template <typename U>
    bool try_push(U&& item) {
        const std::size_t current_head = head_.load(std::memory_order_relaxed);
        const std::size_t current_tail = tail_cache_;

        if (current_head - current_tail >= Capacity) {
            // Buffer appears full, refresh cached tail
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (current_head - tail_cache_ >= Capacity) {
                return false; // Actually full
            }
        }

        buffer_[current_head & BufferMask] = std::forward<U>(item);
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    /// Attempt to dequeue an item. Returns std::nullopt if buffer is empty.
    std::optional<T> try_pop() {
        const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
        const std::size_t current_head = head_cache_;

        if (current_tail == current_head) {
            // Buffer appears empty, refresh cached head
            head_cache_ = head_.load(std::memory_order_acquire);
            if (current_tail == head_cache_) {
                return std::nullopt; // Actually empty
            }
        }

        T item = std::move(buffer_[current_tail & BufferMask]);
        tail_.store(current_tail + 1, std::memory_order_release);
        return item;
    }

    /// Number of items currently available in the buffer
    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        return (h >= t) ? (h - t) : 0;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return Capacity;
    }

private:
    static constexpr std::size_t BufferMask = Capacity - 1;

    // Cache-line aligned storage to prevent false sharing between producer and consumer cores
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::size_t tail_cache_{0};

    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::size_t head_cache_{0};

    alignas(64) std::vector<T> buffer_;
};

} // namespace pulseflow
