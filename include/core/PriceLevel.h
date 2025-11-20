#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "core/OrderArena.h"
#include "types/OrderRequest.h"

class alignas(64) PriceLevel {
public:
    static constexpr size_t kInvalidSlot = std::numeric_limits<size_t>::max();

    PriceLevel() = default;
    PriceLevel(PriceLevel&&) noexcept = default;
    PriceLevel& operator=(PriceLevel&&) noexcept = default;

    size_t addOrder(OrderId orderId, OrderArena& arena);
    bool removeOrderAt(size_t slot, OrderId orderId, OrderArena& arena);

    OrderId headOrderId() const;
    bool empty() const { return count_ == 0; }
    Qty openQty() const { return open_qty_; }
    void decOpenQty(Qty qty);
    void clear();
    void print(const OrderArena& arena) const;

private:
    static constexpr OrderId kInvalidOrder = std::numeric_limits<OrderId>::max();

    struct Node {
        OrderId order = kInvalidOrder;
        size_t next = kInvalidSlot;
        size_t prev = kInvalidSlot;
    };

    std::vector<Node> nodes_;
    size_t head_slot_ = kInvalidSlot;
    size_t tail_slot_ = kInvalidSlot;
    size_t free_head_ = kInvalidSlot;
    size_t count_ = 0;
    Qty open_qty_ = 0;

    size_t allocateSlot();
    void releaseSlot(size_t slot);
    void resetFreeList();
    static Qty pendingQty(OrderId orderId, OrderArena& arena);
};
