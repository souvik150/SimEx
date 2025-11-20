#include "core/PriceLevel.h"

#include <algorithm>
#include <sstream>

#include "utils/LogMacros.h"

size_t PriceLevel::addOrder(OrderId orderId, OrderArena& arena) {
    const size_t slot = allocateSlot();
    auto& node = nodes_[slot];

    node.order = orderId;
    node.prev = tail_slot_;
    node.next = kInvalidSlot;

    if (tail_slot_ != kInvalidSlot) {
        nodes_[tail_slot_].next = slot;
    } else {
        head_slot_ = slot;
    }
    tail_slot_ = slot;

    ++count_;
    open_qty_ += pendingQty(orderId, arena);
    return slot;
}

bool PriceLevel::removeOrderAt(size_t slot, OrderId orderId, OrderArena& arena) {
    if (slot >= nodes_.size()) {
        return false;
    }
    auto& node = nodes_[slot];
    if (node.order != orderId) {
        return false;
    }

    const Qty pending = pendingQty(orderId, arena);
    if (pending >= open_qty_) {
        open_qty_ = 0;
    } else {
        open_qty_ -= pending;
    }

    if (node.prev != kInvalidSlot) {
        nodes_[node.prev].next = node.next;
    } else {
        head_slot_ = node.next;
    }
    if (node.next != kInvalidSlot) {
        nodes_[node.next].prev = node.prev;
    } else {
        tail_slot_ = node.prev;
    }

    releaseSlot(slot);

    if (count_ > 0) {
        --count_;
    }
    if (count_ == 0) {
        head_slot_ = tail_slot_ = kInvalidSlot;
    }

    return true;
}

OrderId PriceLevel::headOrderId() const {
    if (head_slot_ == kInvalidSlot) {
        return kInvalidOrder;
    }
    return nodes_[head_slot_].order;
}

void PriceLevel::decOpenQty(Qty qty) {
    if (qty >= open_qty_) {
        open_qty_ = 0;
    } else {
        open_qty_ -= qty;
    }
}

void PriceLevel::clear() {
    if (!nodes_.empty()) {
        resetFreeList();
    } else {
        free_head_ = kInvalidSlot;
    }
    head_slot_ = tail_slot_ = kInvalidSlot;
    count_ = 0;
    open_qty_ = 0;
}

void PriceLevel::print(const OrderArena& arena) const {
    std::ostringstream out;
    out << "[";
    if (head_slot_ != kInvalidSlot && count_ > 0) {
        size_t idx = head_slot_;
        size_t printed = 0;
        while (idx != kInvalidSlot && printed < count_) {
            const OrderId id = nodes_[idx].order;
            const Order* ord = arena.find(id);
            if (ord) {
                out << ord->orderId() << "(" << ord->pending_quantity() << ")";
            } else {
                out << id << "(?)";
            }
            ++printed;
            idx = nodes_[idx].next;
            if (idx != kInvalidSlot && printed < count_) {
                out << " -> ";
            }
        }
    }
    out << "]";
    LOG_INFO("{}", out.str());
}

Qty PriceLevel::pendingQty(OrderId orderId, OrderArena& arena) {
    Order& order = arena.require(orderId);
    return order.pending_quantity();
}

size_t PriceLevel::allocateSlot() {
    if (free_head_ != kInvalidSlot) {
        const size_t slot = free_head_;
        free_head_ = nodes_[slot].next;
        nodes_[slot].order = kInvalidOrder;
        nodes_[slot].prev = kInvalidSlot;
        nodes_[slot].next = kInvalidSlot;
        return slot;
    }
    const size_t slot = nodes_.size();
    nodes_.push_back(Node{});
    return slot;
}

void PriceLevel::releaseSlot(size_t slot) {
    if (slot >= nodes_.size()) {
        return;
    }
    nodes_[slot].order = kInvalidOrder;
    nodes_[slot].prev = kInvalidSlot;
    nodes_[slot].next = free_head_;
    free_head_ = slot;
}

void PriceLevel::resetFreeList() {
    if (nodes_.empty()) {
        free_head_ = kInvalidSlot;
        return;
    }
    free_head_ = 0;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        nodes_[i].order = kInvalidOrder;
        nodes_[i].prev = kInvalidSlot;
        nodes_[i].next = (i + 1 < nodes_.size()) ? i + 1 : kInvalidSlot;
    }
}
