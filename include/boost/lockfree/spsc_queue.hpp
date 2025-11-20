#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace lockfree {

template <typename T>
class spsc_queue {
public:
    explicit spsc_queue(std::size_t capacity)
        : capacity_(capacity + 1),
          buffer_(capacity_),
          head_(0),
          tail_(0) {}

    bool push(const T& value) {
        return push_impl(value);
    }

    bool push(T&& value) {
        return push_impl(std::move(value));
    }

    bool pop(T& out) {
        const std::size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        out = std::move(buffer_[current_head]);
        head_.store(increment(current_head), std::memory_order_release);
        return true;
    }

    std::size_t read_available() const {
        const std::size_t current_head = head_.load(std::memory_order_acquire);
        const std::size_t current_tail = tail_.load(std::memory_order_acquire);
        if (current_tail >= current_head) {
            return current_tail - current_head;
        }
        return (capacity_ - current_head) + current_tail;
    }

private:
    template <typename U>
    bool push_impl(U&& value) {
        const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next_tail = increment(current_tail);
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[current_tail] = std::forward<U>(value);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    std::size_t increment(std::size_t idx) const {
        ++idx;
        if (idx == capacity_) {
            idx = 0;
        }
        return idx;
    }

    const std::size_t capacity_;
    std::vector<T> buffer_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

} // namespace lockfree
} // namespace boost
