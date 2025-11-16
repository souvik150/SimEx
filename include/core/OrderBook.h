//
// Created by souvik on 11/8/25.
//

#ifndef ORDERMATCHINGSYSTEM_ORDERBOOK_H
#define ORDERMATCHINGSYSTEM_ORDERBOOK_H

#include "PriceLevel.h"
#include "Order.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/OrderBookObserver.h"
#include "core/SideContainer.h"
#include "strategies/MatchingStrategy.h"
#include "utils/MemPool.h"


class OrderBook {
public:
    using TradeListener = std::function<void(const TradeEvent&)>;

    class BookContext final : public MatchingContext {
    public:
        explicit BookContext(OrderBook& book) : book_(book) {}

        PriceLevel* bestLevel(Side side) override;
        void restOrder(std::unique_ptr<Order> order) override;
        void removeRestingOrder(Side restingSide, Price price, PriceLevel& level, OrderId orderId) override;
        void recordTrade(const TradeEvent& event) override;
        Qty availableLiquidityAgainst(Side incomingSide, Price limitPrice) const override;

    private:
        OrderBook& book_;

        PriceLevel* findLevel(Side side, Price price);
        void insertLevel(Side side, Price price, PriceLevel&& level);
        Qty liquidityForBuy(Price limitPrice) const;
        Qty liquidityForSell(Price limitPrice) const;
    };

private:
    std::unique_ptr<SideContainer> bids_;
    std::unique_ptr<SideContainer> asks_;
    bool use_std_map_;

    struct OrderRef {
        Side side;
        Price price;
    };

    // this is used to know in which side of book an order is present
    std::unordered_map<OrderId, OrderRef> order_index_;
    MemPool<PriceLevel::Node> node_pool_;
    BookContext context_;
    std::unordered_map<OrderType, std::unique_ptr<MatchingStrategy>> strategies_;
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
    void setMatchingStrategy(OrderType type, std::unique_ptr<MatchingStrategy> strategy);
    void setTradeListener(TradeListener listener);
    bool cancelOrder(OrderId orderId);
    void modifyOrder(OrderId orderId, Price newPrice, Qty newQty);

    const Order *bestAsk() const;
    const Order *bestBid() const;

    Qty totalOpenQtyAt(Side side, Price price) const;

    void printBook() const;
    void emitTrade(const TradeEvent& event) const;
    MatchingStrategy* strategyFor(OrderType type);

    void setInstrumentToken(InstrumentToken token);
    InstrumentToken instrument_token() const;
    void addObserver(const std::shared_ptr<OrderBookObserver>& observer);
    void snapshot(std::vector<std::pair<Price, Qty>>& bids, std::vector<std::pair<Price, Qty>>& asks) const;
    Price last_trade_price() const;
    Qty last_trade_quantity() const;
};


#endif //ORDERMATCHINGSYSTEM_ORDERBOOK_H
