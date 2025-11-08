//
// Created by souvik on 11/8/25.
//

#ifndef SIMEX_ORDER_H
#define SIMEX_ORDER_H
#include <cstdint>
#include <iostream>

enum class Side {
    INVALID,
    BUY,
    SELL
};

class OrderBuilder;

struct Order {
    friend class OrderBuilder;
private:
    uint64_t order_id_;
    Side side_;
    int64_t price_;
    uint32_t quantity_;
    uint64_t timestamp_;

    Order(const uint64_t id, const Side s, const int64_t p, const uint32_t q, const uint64_t ts)
        : order_id_(id), side_(s), price_(p), quantity_(q), timestamp_(ts) {}

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
    uint64_t orderId() const { return order_id_; }
    Side side() const { return side_; }
    int64_t price() const { return price_; }
    uint32_t quantity() const { return quantity_; }
    uint64_t timestamp() const { return timestamp_; }

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