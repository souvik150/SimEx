//
// Created by souvik on 11/8/25.
//

#ifndef ORDERMATCHINGSYSTEM_ORDERBOOK_H
#define ORDERMATCHINGSYSTEM_ORDERBOOK_H

#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "core/Order.h"
#include "core/OrderArena.h"
#include "core/OrderBookObserver.h"
#include "core/TradeEvent.h"
#include "datastructures/PriceRingBuffer.h"

class OrderBook {
public:
    using TradeListener = std::function<void(const TradeEvent&)>;

private:
    PriceRingBuffer bids_;
    PriceRingBuffer asks_;

    struct OrderRef {
        Side side = Side::INVALID;
        Price price = 0;
        size_t slot = PriceLevel::kInvalidSlot;
    };

    // this is used to know in which side of book an order is present
    static constexpr size_t kOrderIndexChunk = 1024;
    std::vector<OrderRef> order_index_;
    OrderArena orders_;
    TradeListener trade_listener_;
    InstrumentToken instrument_token_ = 0;
    mutable std::vector<std::weak_ptr<OrderBookObserver>> observers_;
    std::vector<TradeEvent> trade_ring_;
    std::atomic<uint64_t> trade_head_{0};
    std::atomic<uint64_t> trade_tail_{0};
    std::atomic<bool> trade_running_{true};
    std::thread trade_thread_;
    std::atomic<Price> last_trade_price_{0};
    std::atomic<Qty> last_trade_qty_{0};
    void tradeWorker();
    void dispatchTrade(const TradeEvent& event);

public:
    explicit OrderBook(bool use_std_map = false);
    ~OrderBook();

    void addOrder(std::unique_ptr<Order> order);
    void processOrder(OrderId orderId);
    void setTradeListener(TradeListener listener);
    bool cancelOrder(OrderId orderId);
    void modifyOrder(OrderId orderId, Price newPrice, Qty newQty);

    const Order *bestAsk() const;
    const Order *bestBid() const;

    Qty totalOpenQtyAt(Side side, Price price) const;

    void printBook() const;
    void emitTrade(const TradeEvent& event) const;

    void setInstrumentToken(InstrumentToken token);
    InstrumentToken instrument_token() const;
    void addObserver(const std::shared_ptr<OrderBookObserver>& observer);
    void snapshot(std::vector<std::pair<Price, Qty>>& bids, std::vector<std::pair<Price, Qty>>& asks) const;
    Price last_trade_price() const;
    Qty last_trade_quantity() const;
    void bindTradeThreadToCores(const std::vector<int>& cores);

private:
    struct MatchParams {
        bool respectPrice = true;
        bool allowRest = true;
    };

    void executeMatch(OrderId orderId, const MatchParams& params);
    void handleIceberg(Order& order);
    bool ensureFokLiquidity(const Order& order) const;
    PriceLevel* bestLevelMutable(Side side);
    const PriceLevel* bestLevelMutable(Side side) const;
    PriceLevel* findLevel(Side side, Price price);
    PriceLevel* ensureLevel(Side side, Price price);
    void eraseLevelIfEmpty(Side side, Price price, PriceLevel& level);
    void restOrderInternal(OrderId orderId);
    void removeRestingOrderInternal(Side restingSide, Price price, PriceLevel& level, OrderId orderId);
    void releaseOrderInternal(OrderId orderId);
    Qty availableLiquidityAgainst(Side incomingSide, Price limitPrice) const;
    Qty liquidityForBuy(Price limitPrice) const;
    Qty liquidityForSell(Price limitPrice) const;
    void ensureOrderIndexCapacity(OrderId orderId);
    OrderRef* findOrderRef(OrderId orderId);
    const OrderRef* findOrderRef(OrderId orderId) const;
    void clearOrderRef(OrderId orderId);
};


#endif //ORDERMATCHINGSYSTEM_ORDERBOOK_H
