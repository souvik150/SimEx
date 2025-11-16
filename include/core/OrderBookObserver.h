#pragma once

class OrderBook;
struct TradeEvent;

class OrderBookObserver {
public:
    virtual ~OrderBookObserver() = default;
    virtual void onTrade(const OrderBook& book, const TradeEvent& event) = 0;
};
