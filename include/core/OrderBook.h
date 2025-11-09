//
// Created by souvik on 11/8/25.
//

#ifndef ORDERMATCHINGSYSTEM_ORDERBOOK_H
#define ORDERMATCHINGSYSTEM_ORDERBOOK_H

#include "PriceLevel.h"
#include "Order.h"
#include <unordered_map>
#include <memory>

#include "datastructures/RBTree.h"


class OrderBook {
private:
    RBTree<Price, PriceLevel, std::greater<Price>> bids_;
    RBTree<Price, PriceLevel, std::less<Price>> asks_;

    struct OrderRef {
        Side side;
        Price price;
    };

    // this is used to know in which side of book an order is present
    std::unordered_map<OrderId, OrderRef> order_index_;

public:
    OrderBook();
    ~OrderBook() = default;

    void addOrder(std::unique_ptr<Order> order);
    bool cancelOrder(OrderId orderId);
    void modifyOrder(OrderId orderId, Price newPrice, Qty newQty);

    const Order *bestAsk() const;
    const Order *bestBid() const;

    void printBook() const;

private:
    void match(std::unique_ptr<Order> incoming, PriceLevel& oppositeLevel, Side incomingSide);
    static HrtTime getTimestamp();
};


#endif //ORDERMATCHINGSYSTEM_ORDERBOOK_H