#pragma once

#include <atomic>
#include <cstdint>

class LatencyStats {
public:
    void observe(uint64_t ns) {
        count_.fetch_add(1, std::memory_order_relaxed);
        total_.fetch_add(ns, std::memory_order_relaxed);
        uint64_t prev = min_.load(std::memory_order_relaxed);
        while (ns < prev && !min_.compare_exchange_weak(prev, ns, std::memory_order_relaxed)) {}
        prev = max_.load(std::memory_order_relaxed);
        while (ns > prev && !max_.compare_exchange_weak(prev, ns, std::memory_order_relaxed)) {}
    }

    void reset() {
        count_.store(0, std::memory_order_relaxed);
        total_.store(0, std::memory_order_relaxed);
        min_.store(UINT64_MAX, std::memory_order_relaxed);
        max_.store(0, std::memory_order_relaxed);
    }

    uint64_t count() const { return count_.load(std::memory_order_relaxed); }
    uint64_t total() const { return total_.load(std::memory_order_relaxed); }
    uint64_t min() const { return min_.load(std::memory_order_relaxed); }
    uint64_t max() const { return max_.load(std::memory_order_relaxed); }
    double average() const {
        const uint64_t cnt = count();
        return cnt ? static_cast<double>(total()) / static_cast<double>(cnt) : 0.0;
    }

private:
    std::atomic<uint64_t> count_{0};
    std::atomic<uint64_t> total_{0};
    std::atomic<uint64_t> min_{UINT64_MAX};
    std::atomic<uint64_t> max_{0};
};
