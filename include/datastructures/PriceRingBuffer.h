#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <limits>

#include "core/PriceLevel.h"
#include "utils/CompilerHints.h"
#include "types/AppTypes.h"
#include "types/OrderSide.h"

class alignas(64) PriceRingBuffer {
public:
    static constexpr size_t kCapacity = 1024;
    static_assert((kCapacity & (kCapacity - 1)) == 0, "PriceRingBuffer capacity must be a power of two");
    static constexpr size_t kInvalidSlot = PriceLevel::kInvalidSlot;

    explicit PriceRingBuffer(Side side);

    PriceLevel* findLevel(Price price);
    const PriceLevel* findLevel(Price price) const;
    PriceLevel* ensureLevel(Price price);
    void eraseLevel(Price price);
    void markLevelNonEmpty(Price price);
    bool bestPrice(Price& out_price) const;

    PriceLevel* bestLevel();
    PriceLevel* bestLevel(Price& out_price);
    const PriceLevel* bestLevel() const;
    const PriceLevel* bestLevel(Price& out_price) const;

    template <typename Fn>
    void forEachAscending(Fn&& fn);

    template <typename Fn>
    void forEachAscending(Fn&& fn) const;

    bool empty() const { return active_levels_ == 0; }
    Qty totalOpenQtyAt(Price price) const;

private:
    struct alignas(64) Slot {
        PriceLevel level;
        Price price = 0;
        bool active = false;
    };

    static_assert(alignof(Slot) == 64, "Slot must be cache-line aligned");

    static constexpr size_t kMask = kCapacity - 1;

    Side side_;
    std::array<Slot, kCapacity> slots_{};
    bool base_initialized_ = false;
    Price base_price_ = 0;
    size_t active_levels_ = 0;
    size_t best_slot_ = kInvalidSlot;
    Price best_price_ = 0;

    void initializeBase(Price price);
    size_t logicalIndex(Price price) const;
    size_t physicalIndex(size_t logical) const;
    size_t slotIndex(Price price) const;
    void updateBestCandidate(size_t slotIdx);
    void recomputeBest() const;
    void recomputeBestInternal();
    bool ensureBestSlot() const;
};

template <typename Fn>
void PriceRingBuffer::forEachAscending(Fn&& fn) {
    for (auto& slot : slots_) {
        if (!slot.active || slot.level.empty()) {
            continue;
        }
        fn(slot.price, slot.level);
    }
}

template <typename Fn>
void PriceRingBuffer::forEachAscending(Fn&& fn) const {
    for (const auto& slot : slots_) {
        if (!slot.active || slot.level.empty()) {
            continue;
        }
        fn(slot.price, slot.level);
    }
}
