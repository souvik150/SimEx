#include "snapshot/SnapshotPublisher.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>

#include "core/OrderBook.h"
#include "utils/LogMacros.h"

namespace {
using namespace std::chrono;
}  // namespace

SnapshotPublisher::SnapshotPublisher(const SnapshotConfig& config,
                                     const std::vector<InstrumentToken>& tokens)
    : config_(config) {
    if (config_.max_levels == 0) {
        config_.max_levels = 1;
    }
    for (auto token : tokens) {
        Region region;
        const std::string name = regionName(config_.shm_prefix, token);
        region.size = snapshot::snapshotBytes(config_.max_levels);
        region.fd = ::shm_open(name.c_str(), O_CREAT | O_RDWR, 0660);
        if (region.fd == -1) {
            LOG_WARN("Failed to open shm {}: {}", name, strerror(errno));
            continue;
        }
        if (::ftruncate(region.fd, static_cast<off_t>(region.size)) == -1) {
            LOG_WARN("ftruncate failed for {}: {}", name, strerror(errno));
            ::close(region.fd);
            continue;
        }
        void* addr = ::mmap(nullptr, region.size, PROT_READ | PROT_WRITE, MAP_SHARED, region.fd, 0);
        if (addr == MAP_FAILED) {
            LOG_WARN("mmap failed for {}: {}", name, strerror(errno));
            ::close(region.fd);
            continue;
        }
        region.ptr = static_cast<snapshot::SharedSnapshot*>(addr);
        region.ptr->max_levels = static_cast<uint32_t>(config_.max_levels);
        region.ptr->bid_count = 0;
        region.ptr->ask_count = 0;
        region.ptr->ltp = 0.0;
        region.ptr->ltq = 0.0;
        region.ptr->sequence.store(0, std::memory_order_relaxed);
        region.next_publish = steady_clock::now();
        regions_[token] = region;
    }
}

SnapshotPublisher::~SnapshotPublisher() {
    for (auto& [token, region] : regions_) {
        if (region.ptr && region.ptr != MAP_FAILED) {
            ::munmap(region.ptr, region.size);
        }
        if (region.fd != -1) {
            ::close(region.fd);
        }
    }
}

void SnapshotPublisher::maybePublish(InstrumentToken token, const OrderBook& book) {
    auto it = regions_.find(token);
    if (it == regions_.end()) {
        return;
    }
    Region& region = it->second;
    const auto now = std::chrono::steady_clock::now();
    if (now < region.next_publish) {
        return;
    }
    region.next_publish = now + config_.interval;
    publishNow(region, book);
}

void SnapshotPublisher::publishNow(Region& region, const OrderBook& book) {
    if (!region.ptr) return;

    std::vector<std::pair<Price, Qty>> bids, asks;
    book.snapshot(bids, asks);

    const auto maxLevels = static_cast<std::size_t>(config_.max_levels);
    const std::size_t bidCount = std::min(maxLevels, bids.size());
    const std::size_t askCount = std::min(maxLevels, asks.size());
    auto* header = region.ptr;
    auto* bidDst = snapshot::bidLevels(header);
    auto* askDst = snapshot::askLevels(header);

    for (std::size_t i = 0; i < bidCount; ++i) {
        bidDst[i].price = static_cast<double>(bids[i].first);
        bidDst[i].qty = static_cast<double>(bids[i].second);
    }
    for (std::size_t i = bidCount; i < maxLevels; ++i) {
        bidDst[i].price = 0.0;
        bidDst[i].qty = 0.0;
    }

    for (std::size_t i = 0; i < askCount; ++i) {
        askDst[i].price = static_cast<double>(asks[i].first);
        askDst[i].qty = static_cast<double>(asks[i].second);
    }
    for (std::size_t i = askCount; i < maxLevels; ++i) {
        askDst[i].price = 0.0;
        askDst[i].qty = 0.0;
    }

    header->bid_count = static_cast<uint32_t>(bidCount);
    header->ask_count = static_cast<uint32_t>(askCount);
    const auto ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header->timestamp_ns = static_cast<uint64_t>(ts);
    header->ltp = static_cast<double>(book.last_trade_price());
    header->ltq = static_cast<double>(book.last_trade_quantity());
    header->sequence.fetch_add(1, std::memory_order_release);
}

std::string SnapshotPublisher::regionName(const std::string& prefix, InstrumentToken token) {
    std::string name = prefix;
    if (name.empty() || name[0] != '/') {
        name = "/" + name;
    }
    name += "_" + std::to_string(token);
    return name;
}
