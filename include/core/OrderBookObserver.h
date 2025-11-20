#pragma once

class OrderBook;
#include "core/TradeEvent.h"

class OrderBookObserver {
public:
    virtual ~OrderBookObserver() = default;
    virtual void onTrade(const OrderBook& book, const TradeEvent& event) = 0;
};
