#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace snapshot {

constexpr uint32_t kSnapshotMagic = 0x5349424b;  // 'SIBK' signature
constexpr uint32_t kSnapshotVersion = 1;

struct Level {
    double price = 0.0;
    double qty = 0.0;
};

struct alignas(64) SharedSnapshot {
    std::atomic<uint64_t> sequence{0};
    uint32_t max_levels = 0;
    uint32_t bid_count = 0;
    uint32_t ask_count = 0;
    uint64_t timestamp_ns = 0;
    double ltp = 0.0;
    double ltq = 0.0;
    Level data[1];
};

inline Level* bidLevels(SharedSnapshot* snapshot) {
    return snapshot->data;
}

inline Level* askLevels(SharedSnapshot* snapshot) {
    return snapshot->data + snapshot->max_levels;
}

inline std::size_t snapshotBytes(std::size_t max_levels) {
    const std::size_t levels = std::max<std::size_t>(1, max_levels);
    return sizeof(SharedSnapshot) + sizeof(Level) * (levels * 2 - 1);
}

}  // namespace snapshot
