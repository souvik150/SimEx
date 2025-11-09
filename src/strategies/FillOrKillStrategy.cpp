#include "strategies/FillOrKillStrategy.h"

#include <limits>

#include "core/Order.h"

FillOrKillStrategy::FillOrKillStrategy()
    : executor_(PriceTimeStrategy::PriceMode::RespectLimit,
                PriceTimeStrategy::ResidualMode::Cancel) {}

void FillOrKillStrategy::execute(std::unique_ptr<Order> order, MatchingContext& context) {
    if (!order)
        return;

    const Qty required = order->pending_quantity();
    const Price limit = order->price();
    const Qty available = context.availableLiquidityAgainst(order->side(), limit);

    if (available < required)
        return;

    executor_.execute(std::move(order), context);
}
