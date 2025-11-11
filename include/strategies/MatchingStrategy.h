#pragma once

#include <memory>

#include "types/AppTypes.h"
#include "types/OrderSide.h"

class Order;
class PriceLevel;

struct TradeEvent {
    InstrumentToken instrument;
    Side aggressorSide;
    OrderId aggressorId;
    Side restingSide;
    OrderId restingOrderId;
    Price price;
    Qty quantity;
};

class MatchingContext {
public:
    virtual ~MatchingContext() = default;

    virtual PriceLevel* bestLevel(Side side) = 0;
    virtual void restOrder(std::unique_ptr<Order> order) = 0;
    virtual void removeRestingOrder(Side restingSide, Price price, PriceLevel& level, OrderId orderId) = 0;
    virtual void recordTrade(const TradeEvent& event) = 0;
    virtual Qty availableLiquidityAgainst(Side incomingSide, Price limitPrice) const = 0;
};

class MatchingStrategy {
public:
    virtual ~MatchingStrategy() = default;
    virtual void execute(std::unique_ptr<Order> order, MatchingContext& context) = 0;
};
