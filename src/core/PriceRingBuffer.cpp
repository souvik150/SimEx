#include "datastructures/PriceRingBuffer.h"

#include <algorithm>
#include <limits>

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
    if (UNLIKELY(!base_initialized_)) {
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
    if (UNLIKELY(!base_initialized_)) {
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
    if (UNLIKELY(!base_initialized_)) {
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
