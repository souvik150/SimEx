//
// Created by souvik on 11/8/25.
//

#ifndef ORDERMATCHINGSYSTEM_ORDERBUILDER_H
#define ORDERMATCHINGSYSTEM_ORDERBUILDER_H
#include <cstdint>
#include "Order.h"

class OrderBuilder {
private:
    OrderId order_id_;
    Price price_;
    HrtTime timestamp_;
    Qty quantity_;
    Qty display_quantity_ = 0;
    InstrumentToken instrument_token_ = 0;
    OrderType order_type_ = OrderType::LIMIT;
    Side side_ = Side::INVALID;

public:
    OrderBuilder& setOrderId(OrderId id) {
        order_id_ = id;
        return *this;
    }

    OrderBuilder& setSide(Side s) {
        side_ = s;
        return *this;
    }

    OrderBuilder& setPrice(Price p) {
        price_ = p;
        return *this;
    }

    OrderBuilder& setQuantity(Qty q) {
        quantity_ = q;
        return *this;
    }

    OrderBuilder& setInstrumentToken(InstrumentToken token) {
        instrument_token_ = token;
        return *this;
    }

    OrderBuilder& setTimestamp(HrtTime ts) {
        timestamp_ = ts;
        return *this;
    }

    OrderBuilder& setOrderType(OrderType type) {
        order_type_ = type;
        return *this;
    }

    OrderBuilder& setDisplayQuantity(Qty displayQty) {
        display_quantity_ = displayQty;
        return *this;
    }

    [[nodiscard]] std::unique_ptr<Order> build() {
        // if (!order_id_ || side_== Side::INVALID || !price_ || !quantity_)
        //     throw std::runtime_error("Missing required order fields");

        return std::unique_ptr<Order>(new Order(order_id_, instrument_token_, side_, price_, quantity_, timestamp_, order_type_, display_quantity_));
    }
};

#endif //ORDERMATCHINGSYSTEM_ORDERBUILDER_H
