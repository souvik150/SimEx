//
// Created by souvik on 11/8/25.
//

#ifndef SIMEX_ORDER_H
#define SIMEX_ORDER_H
#include <chrono>
#include <cstdint>
#include <iostream>

#include "types/AppTypes.h"
#include "types/OrderSide.h"

class OrderBuilder;

struct Order {
    friend class OrderBuilder;
private:
    OrderId order_id_;
    Side side_;
    Price price_;
    Qty quantity_;
    Qty filled_quantity_;
    uint32_t user_id_;
    HrtTime timestamp_;

    Order( OrderId id,  Side s,  Price p,  Qty q,  HrtTime ts)
        : order_id_(id), side_(s), price_(p), quantity_(q), filled_quantity_(0), timestamp_(ts) {
    }

public:
    // call using builder
    Order() = delete;

    // delete copy constructors
    Order(const Order&) = delete;
    Order& operator = (const Order&) = delete;

    // default move constructors
    Order(Order&&) noexcept = default;
    Order* operator==(Order&&) noexcept = delete;

    // accessors
    OrderId orderId() const { return order_id_; }
    Side side() const { return side_; }
    Price price() const { return price_; }
    Qty quantity() const { return quantity_; }
    Qty filled_quantity() const { return filled_quantity_; }
    Qty pending_quantity() const { return quantity_ - filled_quantity_; }
    HrtTime timestamp() const { return timestamp_; }

    bool modifyQty(const Qty newOrderQty) {
        if (const uint32_t remainingQty = quantity_ - filled_quantity_; newOrderQty < remainingQty) {
            throw std::runtime_error("invalid mod");
            return false;
        }
        quantity_ = newOrderQty;
        timestamp_ = std::chrono::high_resolution_clock::now();
        return true;
    }

    bool addFill(const Qty filledQty) {
        filled_quantity_ += filledQty;
        timestamp_ = std::chrono::high_resolution_clock::now();
        return true;
    }

    void modifyPrice(const Price newPrice) {
        price_ = newPrice;
        timestamp_ = std::chrono::high_resolution_clock::now();
    }

    void print() const {
        std::cout << "Order{id=" << order_id_
                  << ", side=" << (side_ == Side::BUY ? "BUY" : "SELL")
                  << ", price=" << price_
                  << ", qty=" << quantity_
                  << ", ts=" << timestamp_
                  << "}" << std::endl;
    }
};


#endif //SIMEX_ORDER_H