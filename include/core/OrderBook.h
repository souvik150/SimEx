//
// Created by souvik on 11/8/25.
//

#ifndef ORDERMATCHINGSYSTEM_ORDERBOOK_H
#define ORDERMATCHINGSYSTEM_ORDERBOOK_H

#include "PriceLevel.h"
#include "Order.h"
#include <functional>
#include <memory>
#include <unordered_map>

#include "datastructures/RBTree.h"
#include "strategies/MatchingStrategy.h"


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
    RBTree<Price, PriceLevel, std::greater<Price>> bids_;
    RBTree<Price, PriceLevel, std::less<Price>> asks_;

    struct OrderRef {
        Side side;
        Price price;
    };

    // this is used to know in which side of book an order is present
    std::unordered_map<OrderId, OrderRef> order_index_;
    BookContext context_;
    std::unordered_map<OrderType, std::unique_ptr<MatchingStrategy>> strategies_;
    TradeListener trade_listener_;

public:
    OrderBook();
    ~OrderBook() = default;

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
};


#endif //ORDERMATCHINGSYSTEM_ORDERBOOK_H
