#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "snapshot/SnapshotLayout.h"
#include "types/AppTypes.h"

class OrderBook;

struct SnapshotConfig {
    std::string shm_prefix = "/simex_book";
    std::chrono::milliseconds interval{50};
    std::size_t max_levels = 32;
};

class SnapshotPublisher {
public:
    explicit SnapshotPublisher(const SnapshotConfig& config,
                               const std::vector<InstrumentToken>& tokens);
    ~SnapshotPublisher();

    void maybePublish(InstrumentToken token, const OrderBook& book);

private:
    struct Region {
        int fd = -1;
        std::size_t size = 0;
        snapshot::SharedSnapshot* ptr = nullptr;
        std::chrono::steady_clock::time_point next_publish{};
    };

    SnapshotConfig config_;
    std::unordered_map<InstrumentToken, Region> regions_;

    static std::string regionName(const std::string& prefix, InstrumentToken token);
    void publishNow(Region& region, const OrderBook& book);
};
