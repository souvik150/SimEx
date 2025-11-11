#include "strategies/PriceTimeStrategy.h"

#include <algorithm>

#include "core/Order.h"
#include "core/PriceLevel.h"

void PriceTimeStrategy::execute(std::unique_ptr<Order> order, MatchingContext& context) {
    if (!order)
        return;

    const Side incomingSide = order->side();
    const Side oppositeSide = (incomingSide == Side::BUY) ? Side::SELL : Side::BUY;

    const bool respectPrice = price_mode_ == PriceMode::RespectLimit;
    const bool allowRest = residual_mode_ == ResidualMode::Rest;

    while (order->pending_quantity() > 0) {
        PriceLevel* oppositeLevel = context.bestLevel(oppositeSide);
        if (!oppositeLevel)
            break;

        Order* headOrder = oppositeLevel->headOrder();
        if (!headOrder)
            break;

        bool matchPossible = true;
        if (respectPrice) {
            matchPossible = (incomingSide == Side::BUY)
                ? order->price() >= headOrder->price()
                : order->price() <= headOrder->price();
        }

        if (!matchPossible)
            break;

        const Qty tradeQty = std::min(order->pending_quantity(), headOrder->pending_quantity());
        const Price tradePrice = headOrder->price();
        const OrderId restingId = headOrder->orderId();

        order->addFill(tradeQty);
        oppositeLevel->addFill(tradeQty);

        context.recordTrade(TradeEvent{
            .aggressorSide = incomingSide,
            .aggressorId = order->orderId(),
            .restingSide = oppositeSide,
            .restingOrderId = restingId,
            .price = tradePrice,
            .quantity = tradeQty,
        });

        if (headOrder->pending_quantity() == 0) {
            context.removeRestingOrder(oppositeSide, tradePrice, *oppositeLevel, restingId);
        }
    }

    //  turns an order that wasn’t fully filled right now into resting depth available for future matches
    if (allowRest && order && order->pending_quantity() > 0) {
        order->refreshWorkingQuantity();
        context.restOrder(std::move(order));
    }
}
