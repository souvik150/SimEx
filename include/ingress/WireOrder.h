#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <spdlog/fmt/fmt.h>

#include "types/AppTypes.h"
#include "types/OrderSide.h"
#include "types/OrderType.h"

namespace ingress {

struct WireOrder {
    OrderId order_id = 0;
    InstrumentToken instrument = 0;
    Side side = Side::INVALID;
    Price price = 0;
    Qty quantity = 0;
    OrderType type = OrderType::LIMIT;
    Qty display = 0;
};

inline std::string_view toString(Side side) {
    switch (side) {
        case Side::BUY: return "BUY";
        case Side::SELL: return "SELL";
        default: return "INVALID";
    }
}

inline std::string_view toString(OrderType type) {
    switch (type) {
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::MARKET: return "MARKET";
        case OrderType::IOC: return "IOC";
        case OrderType::FOK: return "FOK";
        case OrderType::ICEBERG: return "ICEBERG";
        default: return "LIMIT";
    }
}

inline std::optional<Side> sideFromString(std::string_view value) {
    if (value == "BUY") return Side::BUY;
    if (value == "SELL") return Side::SELL;
    return std::nullopt;
}

inline std::optional<OrderType> orderTypeFromString(std::string_view value) {
    if (value == "LIMIT") return OrderType::LIMIT;
    if (value == "MARKET") return OrderType::MARKET;
    if (value == "IOC") return OrderType::IOC;
    if (value == "FOK") return OrderType::FOK;
    if (value == "ICEBERG") return OrderType::ICEBERG;
    return std::nullopt;
}

inline std::string serializeWireOrder(const WireOrder& order) {
    return fmt::format("{},{},{},{},{},{},{}",
                       order.order_id,
                       order.instrument,
                       toString(order.side),
                       order.price,
                       order.quantity,
                       toString(order.type),
                       order.display);
}

inline bool parseWireOrder(std::string_view line, WireOrder& out) {
    std::array<std::string_view, 7> parts{};
    size_t start = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        size_t end = (i + 1 == parts.size()) ? line.size() : line.find(',', start);
        if (end == std::string_view::npos) {
            return false;
        }
        parts[i] = line.substr(start, end - start);
        start = end + 1;
    }

    try {
        out.order_id = static_cast<OrderId>(std::stoull(std::string(parts[0])));
        out.instrument = static_cast<InstrumentToken>(std::stoul(std::string(parts[1])));
        auto maybeSide = sideFromString(parts[2]);
        if (!maybeSide) return false;
        out.side = *maybeSide;
        out.price = static_cast<Price>(std::stoull(std::string(parts[3])));
        out.quantity = static_cast<Qty>(std::stoul(std::string(parts[4])));
        auto maybeType = orderTypeFromString(parts[5]);
        if (!maybeType) return false;
        out.type = *maybeType;
        out.display = static_cast<Qty>(std::stoul(std::string(parts[6])));
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

} // namespace ingress
