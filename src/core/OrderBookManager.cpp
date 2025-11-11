#include "core/OrderBookManager.h"

#include <utility>

#include "utils/LogMacros.h"

OrderBook& OrderBookManager::ensureBook(InstrumentToken token) {
    auto& entry = books_[token];
    if (!entry) {
        entry = std::make_unique<OrderBook>();
    }
    return *entry;
}

void OrderBookManager::addOrder(std::unique_ptr<Order> order) {
    if (!order) {
        return;
    }

    const InstrumentToken token = order->instrument_token();
    if (token == 0) {
        LOG_WARN("Ignoring order {} without instrument token", order->orderId());
        return;
    }

    ensureBook(token).addOrder(std::move(order));
}

bool OrderBookManager::cancelOrder(InstrumentToken token, OrderId orderId) {
    if (auto* book = findBook(token)) {
        return book->cancelOrder(orderId);
    }
    return false;
}

void OrderBookManager::modifyOrder(InstrumentToken token, OrderId orderId, Price newPrice, Qty newQty) {
    if (auto* book = findBook(token)) {
        book->modifyOrder(orderId, newPrice, newQty);
    }
}

const Order* OrderBookManager::bestBid(InstrumentToken token) const {
    if (const auto* book = findBook(token)) {
        return book->bestBid();
    }
    return nullptr;
}

const Order* OrderBookManager::bestAsk(InstrumentToken token) const {
    if (const auto* book = findBook(token)) {
        return book->bestAsk();
    }
    return nullptr;
}

Qty OrderBookManager::totalOpenQtyAt(InstrumentToken token, Side side, Price price) const {
    if (const auto* book = findBook(token)) {
        return book->totalOpenQtyAt(side, price);
    }
    return 0;
}

void OrderBookManager::setTradeListener(InstrumentToken token, OrderBook::TradeListener listener) {
    ensureBook(token).setTradeListener(std::move(listener));
}

OrderBook* OrderBookManager::findBook(InstrumentToken token) {
    auto it = books_.find(token);
    return it != books_.end() ? it->second.get() : nullptr;
}

const OrderBook* OrderBookManager::findBook(InstrumentToken token) const {
    auto it = books_.find(token);
    return it != books_.end() ? it->second.get() : nullptr;
}
