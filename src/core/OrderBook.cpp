//
// Created by souvik on 11/8/25.
//

#include "core/OrderBook.h"

OrderBook::OrderBook() = default;

void OrderBook::addOrder(std::unique_ptr<Order> order) {
    Price px = order->price();
    Side side = order->side();
    OrderId orderId = order->orderId();

    if (side == Side::BUY) {
        auto bestAsk = asks_.best();
        if (bestAsk && px >= bestAsk->headOrder()->price()) {
            match(std::move(order), *bestAsk, Side::BUY);
            return;
        }

        if (auto* level = bids_.find(px); !level) {
            PriceLevel pl;
            pl.addOrder(std::move(order));
            bids_.insert(orderId, std::move(pl));
        } else {
            level->addOrder(std::move(order));
        }
        order_index_[orderId] = {Side::BUY, px};
    }
}
