#include "datastructures/PriceRingBuffer.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace {
constexpr size_t kHalfCapacity = PriceRingBuffer::kCapacity / 2;
}

PriceRingBuffer::PriceRingBuffer(Side side)
    : side_(side) {
    for (auto& slot : slots_) {
        slot.price = 0;
        slot.active = false;
    }
}

PriceLevel* PriceRingBuffer::findLevel(Price price) {
    if (UNLIKELY(!base_initialized_ || !priceInWindow(price))) {
        return nullptr;
    }
    const size_t idx = slotIndex(price);
    auto& slot = slots_[idx];
    if (UNLIKELY(!slot.active || slot.price != price)) {
        return nullptr;
    }
    return &slot.level;
}

const PriceLevel* PriceRingBuffer::findLevel(Price price) const {
    if (UNLIKELY(!base_initialized_ || !priceInWindow(price))) {
        return nullptr;
    }
    const size_t idx = slotIndex(price);
    const auto& slot = slots_[idx];
    if (UNLIKELY(!slot.active || slot.price != price)) {
        return nullptr;
    }
    return &slot.level;
}

PriceLevel* PriceRingBuffer::ensureLevel(Price price) {
    if (UNLIKELY(!base_initialized_)) {
        initializeBase(price);
    }

    if (UNLIKELY(!priceInWindow(price))) {
        rebalanceWindow(price);
    }

    if (UNLIKELY(!priceInWindow(price))) {
        return nullptr;
    }

    const size_t idx = slotIndex(price);
    auto& slot = slots_[idx];
    if (LIKELY(!slot.active)) {
        slot.active = true;
        slot.price = price;
        slot.level.clear();
        ++active_levels_;
    } else if (UNLIKELY(slot.price != price)) {
        if (UNLIKELY(!slot.level.empty())) {
            return nullptr;
        }
        slot.price = price;
        slot.level.clear();
    }
    updateBestCandidate(idx);
    return &slot.level;
}

void PriceRingBuffer::eraseLevel(Price price) {
    auto* level = findLevel(price);
    if (!level) {
        return;
    }
    const size_t idx = slotIndex(price);
    auto& slot = slots_[idx];
    slot.level.clear();
    slot.active = false;
    if (LIKELY(active_levels_ > 0)) {
        --active_levels_;
    }
    if (best_slot_ == idx) {
        best_slot_ = kInvalidSlot;
        recomputeBestInternal();
    }
}

void PriceRingBuffer::markLevelNonEmpty(Price price) {
    if (UNLIKELY(!base_initialized_ || !priceInWindow(price))) {
        return;
    }
    const size_t idx = slotIndex(price);
    updateBestCandidate(idx);
}

PriceLevel* PriceRingBuffer::bestLevel() {
    Price ignored = 0;
    return bestLevel(ignored);
}

PriceLevel* PriceRingBuffer::bestLevel(Price& out_price) {
    if (!ensureBestSlot()) {
        return nullptr;
    }
    out_price = best_price_;
    return &slots_[best_slot_].level;
}

const PriceLevel* PriceRingBuffer::bestLevel() const {
    Price ignored = 0;
    return bestLevel(ignored);
}

const PriceLevel* PriceRingBuffer::bestLevel(Price& out_price) const {
    if (!ensureBestSlot()) {
        return nullptr;
    }
    out_price = best_price_;
    return &slots_[best_slot_].level;
}

bool PriceRingBuffer::bestPrice(Price& out_price) const {
    return bestLevel(out_price) != nullptr;
}

Qty PriceRingBuffer::totalOpenQtyAt(Price price) const {
    const PriceLevel* level = findLevel(price);
    return level ? level->openQty() : 0;
}

void PriceRingBuffer::initializeBase(Price price) {
    base_price_ = (price > kHalfCapacity) ? price - kHalfCapacity : 0;
    base_price_ = clampBase(base_price_);
    base_initialized_ = true;
    for (size_t i = 0; i < slots_.size(); ++i) {
        slots_[i].price = base_price_ + i;
        slots_[i].active = false;
        slots_[i].level.clear();
    }
    active_levels_ = 0;
    best_slot_ = kInvalidSlot;
    best_price_ = (side_ == Side::BUY) ? 0 : std::numeric_limits<Price>::max();
}

size_t PriceRingBuffer::logicalIndex(Price price) const {
    return static_cast<size_t>(price - base_price_);
}

size_t PriceRingBuffer::physicalIndex(size_t logical) const {
    return logical & kMask;
}

size_t PriceRingBuffer::slotIndex(Price price) const {
    const size_t logical = logicalIndex(price);
    return physicalIndex(logical);
}

bool PriceRingBuffer::priceInWindow(Price price) const {
    if (UNLIKELY(!base_initialized_)) {
        return false;
    }
    const Price upper_inclusive = base_price_ + static_cast<Price>(kCapacity - 1);
    return price >= base_price_ && price <= upper_inclusive;
}

Price PriceRingBuffer::clampBase(Price candidate) const {
    const Price max_base = std::numeric_limits<Price>::max() - static_cast<Price>(kCapacity - 1);
    return (candidate > max_base) ? max_base : candidate;
}

void PriceRingBuffer::rebalanceWindow(Price focus_price) {
    if (UNLIKELY(!base_initialized_)) {
        initializeBase(focus_price);
        return;
    }

    ensureBestSlot();
    Price reference_price = focus_price;
    if (best_slot_ != kInvalidSlot) {
        reference_price = best_price_;
    }

    Price new_base = (reference_price > kHalfCapacity)
        ? reference_price - kHalfCapacity
        : 0;
    new_base = clampBase(new_base);

    const Price span_minus_one = static_cast<Price>(kCapacity - 1);

    auto focusAnchoredBase = [&](Price price) -> Price {
        if (price <= span_minus_one) {
            return 0;
        }
        return clampBase(price - span_minus_one);
    };

    if (focus_price < new_base) {
        const Price diff = new_base - focus_price;
        if (diff > span_minus_one) {
            new_base = focusAnchoredBase(focus_price);
        } else {
            new_base -= diff;
        }
    } else {
        const Price upper_inclusive = new_base + span_minus_one;
        if (focus_price > upper_inclusive) {
            const Price diff = focus_price - upper_inclusive;
            if (diff > span_minus_one) {
                new_base = focusAnchoredBase(focus_price);
            } else {
                new_base += diff;
            }
        }
    }

    new_base = clampBase(new_base);
    const Price new_upper_inclusive = new_base + span_minus_one;

    if (UNLIKELY(new_base == base_price_ && priceInWindow(focus_price))) {
        return;
    }

    std::array<Slot, kCapacity> new_slots;
    for (size_t i = 0; i < kCapacity; ++i) {
        new_slots[i].price = new_base + static_cast<Price>(i);
        new_slots[i].active = false;
    }

    size_t new_active_count = 0;
    for (auto& slot : slots_) {
        if (!slot.active) {
            continue;
        }
        const Price slot_price = slot.price;
        if (slot_price < new_base || slot_price > new_upper_inclusive) {
            slot.level.clear();
            slot.active = false;
            continue;
        }
        const size_t new_idx = static_cast<size_t>(slot_price - new_base);
        auto& dest = new_slots[new_idx];
        dest.level = std::move(slot.level);
        dest.price = slot_price;
        dest.active = true;
        ++new_active_count;
    }

    slots_ = std::move(new_slots);
    base_price_ = new_base;
    active_levels_ = new_active_count;
    best_slot_ = kInvalidSlot;
    best_price_ = (side_ == Side::BUY) ? 0 : std::numeric_limits<Price>::max();
    recomputeBestInternal();
}

void PriceRingBuffer::updateBestCandidate(size_t slotIdx) {
    if (slotIdx >= slots_.size()) {
        return;
    }
    auto& slot = slots_[slotIdx];
    if (!slot.active || slot.level.empty()) {
        if (slotIdx == best_slot_) {
            best_slot_ = kInvalidSlot;
        }
        return;
    }

    if (best_slot_ == kInvalidSlot) {
        best_slot_ = slotIdx;
        best_price_ = slot.price;
        return;
    }

    if (side_ == Side::BUY) {
        if (slot.price > best_price_) {
            best_slot_ = slotIdx;
            best_price_ = slot.price;
        }
    } else {
        if (slot.price < best_price_) {
            best_slot_ = slotIdx;
            best_price_ = slot.price;
        }
    }
}

void PriceRingBuffer::recomputeBest() const {
    const_cast<PriceRingBuffer*>(this)->recomputeBestInternal();
}

void PriceRingBuffer::recomputeBestInternal() {
    best_slot_ = kInvalidSlot;
    best_price_ = (side_ == Side::BUY) ? 0 : std::numeric_limits<Price>::max();

    for (size_t idx = 0; idx < slots_.size(); ++idx) {
        const auto& slot = slots_[idx];
        if (!slot.active || slot.level.empty()) {
            continue;
        }
        if (best_slot_ == kInvalidSlot) {
            best_slot_ = idx;
            best_price_ = slot.price;
            continue;
        }

        if (side_ == Side::BUY) {
            if (slot.price > best_price_) {
                best_slot_ = idx;
                best_price_ = slot.price;
            }
        } else {
            if (slot.price < best_price_) {
                best_slot_ = idx;
                best_price_ = slot.price;
            }
        }
    }
}

bool PriceRingBuffer::ensureBestSlot() const {
    if (best_slot_ == kInvalidSlot) {
        recomputeBest();
    }
    if (best_slot_ == kInvalidSlot) {
        return false;
    }
    const auto& slot = slots_[best_slot_];
    if (!slot.active || slot.level.empty()) {
        const_cast<PriceRingBuffer*>(this)->best_slot_ = kInvalidSlot;
        const_cast<PriceRingBuffer*>(this)->recomputeBestInternal();
        return best_slot_ != kInvalidSlot;
    }
    const_cast<PriceRingBuffer*>(this)->best_price_ = slot.price;
    return true;
}
