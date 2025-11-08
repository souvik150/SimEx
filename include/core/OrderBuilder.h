//
// Created by souvik on 11/8/25.
//

#ifndef ORDERMATCHINGSYSTEM_ORDERBUILDER_H
#define ORDERMATCHINGSYSTEM_ORDERBUILDER_H
#include <cstdint>
#include "Order.h"

class OrderBuilder {
private:
    uint64_t order_id_;
    Side side_;
    int64_t price_;
    uint32_t quantity_;
    uint64_t timestamp_;

public:
    OrderBuilder& setOrderId(uint64_t id) {
        order_id_ = id;
        return *this;
    }

    OrderBuilder& setSide(Side s) {
        side_ = s;
        return *this;
    }

    OrderBuilder& setPrice(double p) {
        price_ = p;
        return *this;
    }

    OrderBuilder& setQuantity(uint32_t q) {
        quantity_ = q;
        return *this;
    }

    OrderBuilder& setTimestamp(uint64_t ts) {
        timestamp_ = ts;
        return *this;
    }

    Order build() const {
        if (!order_id_ || side_== Side::INVALID || !price_ || !quantity_ || !timestamp_)
            throw std::runtime_error("Missing required order fields");

        if (price_ <= 0)
            throw std::runtime_error("Invalid price: must be > 0");


        return Order(order_id_, side_, price_, quantity_, timestamp_);
    }
};

#endif //ORDERMATCHINGSYSTEM_ORDERBUILDER_H