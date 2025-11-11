#pragma once

#include <memory>
#include <unordered_map>

#include "core/OrderBook.h"

class OrderBookManager {
public:
    void addOrder(std::unique_ptr<Order> order);
    bool cancelOrder(InstrumentToken token, OrderId orderId);
    void modifyOrder(InstrumentToken token, OrderId orderId, Price newPrice, Qty newQty);

    const Order* bestBid(InstrumentToken token) const;
    const Order* bestAsk(InstrumentToken token) const;
    Qty totalOpenQtyAt(InstrumentToken token, Side side, Price price) const;

    void setTradeListener(InstrumentToken token, OrderBook::TradeListener listener);

    OrderBook* findBook(InstrumentToken token);
    const OrderBook* findBook(InstrumentToken token) const;

private:
    OrderBook& ensureBook(InstrumentToken token);

    std::unordered_map<InstrumentToken, std::unique_ptr<OrderBook>> books_;
};
