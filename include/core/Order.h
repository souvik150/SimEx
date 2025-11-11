//
// Created by souvik on 11/8/25.
//

#ifndef SIMEX_ORDER_H
#define SIMEX_ORDER_H
#include <algorithm>
#include <chrono>
#include <cstdint>

#include "types/AppTypes.h"
#include "types/OrderSide.h"
#include "types/OrderType.h"
#include "utils/Logger.h"

class OrderBuilder;

class Order {
    friend class OrderBuilder;
private:
    OrderId order_id_;
    Price price_;
    HrtTime timestamp_;
    InstrumentToken instrument_token_;
    Qty total_quantity_;
    Qty working_quantity_;
    Qty filled_quantity_;
    Qty display_quantity_;
    Side side_;
    OrderType type_;
    uint32_t user_id_ = 0;

    Order(OrderId id, InstrumentToken instrument, Side s, Price p, Qty q, HrtTime ts, OrderType type, Qty display_qty)
        : order_id_(id),
          price_(p),
          timestamp_(ts),
          instrument_token_(instrument),
          total_quantity_(q),
          working_quantity_(q),
          filled_quantity_(0),
          display_quantity_(display_qty),
          side_(s),
          type_(type),
          user_id_(0) {
    }

    static const char* printOrderType(OrderType type) {
        switch (type) {
            case OrderType::LIMIT: return "LIMIT";
            case OrderType::MARKET: return "MARKET";
            case OrderType::IOC: return "IOC";
            case OrderType::FOK: return "FOK";
            case OrderType::ICEBERG: return "ICEBERG";
            default: return "UNKNOWN";
        }
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
    InstrumentToken instrument_token() const { return instrument_token_; }
    Qty quantity() const { return total_quantity_; }
    Qty workingQuantity() const { return working_quantity_; }
    Qty filled_quantity() const { return filled_quantity_; }
    Qty pending_quantity() const {
        if (working_quantity_ < filled_quantity_) return 0;
        return working_quantity_ - filled_quantity_;
    }
    HrtTime timestamp() const { return timestamp_; }
    OrderType type() const { return type_; }
    Qty display_quantity() const { return display_quantity_; }
    bool hasDisplayQuantity() const { return display_quantity_ > 0 && type_ == OrderType::ICEBERG; }
    Qty remaining_quantity() const {
        if (total_quantity_ < filled_quantity_) return 0;
        return total_quantity_ - filled_quantity_;
    }

    bool modifyQty(const Qty newOrderQty) {
        if (newOrderQty < filled_quantity_) {
            return false;
        }
        total_quantity_ = newOrderQty;
        refreshWorkingQuantity();
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

    void setOrderType(OrderType type) { type_ = type; }

    void setDisplayQuantity(Qty displayQty) {
        display_quantity_ = displayQty;
    }

    void refreshWorkingQuantity() {
        if (hasDisplayQuantity()) {
            const Qty remaining = remaining_quantity();
            if (remaining == 0) {
                working_quantity_ = filled_quantity_;
                return;
            }
            const Qty clip = std::min(display_quantity_, remaining);
            working_quantity_ = filled_quantity_ + clip;
        } else {
            working_quantity_ = total_quantity_;
        }
    }

    void print() const {
        logging::logger().info(
            "Order{{id={}, token={}, side={}, type={}, price={}, qty={}, display={}, ts={}}}",
            order_id_,
            instrument_token_,
            (side_ == Side::BUY ? "BUY" : "SELL"),
            printOrderType(type_),
            (type_ == OrderType::MARKET ? 0 : price_),
            total_quantity_,
            display_quantity_,
            timestamp_.time_since_epoch().count());
    }
};


#endif //SIMEX_ORDER_H
