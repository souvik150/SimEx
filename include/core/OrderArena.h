#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "core/Order.h"

class alignas(64) OrderArena {
public:
    OrderArena();

    OrderArena(const OrderArena&) = delete;
    OrderArena& operator=(const OrderArena&) = delete;

    Order& store(std::unique_ptr<Order> order);
    Order* find(OrderId id);
    const Order* find(OrderId id) const;
    Order& require(OrderId id);
    const Order& require(OrderId id) const;
    void erase(OrderId id);

private:
    static constexpr size_t kChunkSize = 512;

    std::vector<std::unique_ptr<Order>> slots_;

    void ensureCapacity(OrderId id);
};
