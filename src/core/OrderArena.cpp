#include "core/OrderArena.h"

#include <stdexcept>

OrderArena::OrderArena() {
    slots_.reserve(kChunkSize);
}

Order& OrderArena::store(std::unique_ptr<Order> order) {
    if (!order) {
        throw std::invalid_argument("OrderArena::store received null order");
    }
    const OrderId id = order->orderId();
    ensureCapacity(id);
    slots_[id] = std::move(order);
    return *slots_[id];
}

Order* OrderArena::find(OrderId id) {
    if (id >= slots_.size()) {
        return nullptr;
    }
    return slots_[id].get();
}

const Order* OrderArena::find(OrderId id) const {
    if (id >= slots_.size()) {
        return nullptr;
    }
    return slots_[id].get();
}

Order& OrderArena::require(OrderId id) {
    auto* ptr = find(id);
    if (!ptr) {
        throw std::out_of_range("OrderArena::require missing order");
    }
    return *ptr;
}

const Order& OrderArena::require(OrderId id) const {
    auto* ptr = find(id);
    if (!ptr) {
        throw std::out_of_range("OrderArena::require missing order");
    }
    return *ptr;
}

void OrderArena::erase(OrderId id) {
    if (id < slots_.size()) {
        slots_[id].reset();
    }
}

void OrderArena::ensureCapacity(OrderId id) {
    if (id < slots_.size()) {
        return;
    }
    const size_t required = static_cast<size_t>(id) + 1;
    size_t newSize = ((required + kChunkSize - 1) / kChunkSize) * kChunkSize;
    slots_.resize(newSize);
}
