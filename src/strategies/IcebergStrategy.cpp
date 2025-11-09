#include "strategies/IcebergStrategy.h"

#include "core/Order.h"

IcebergStrategy::IcebergStrategy()
    : executor_(PriceTimeStrategy::PriceMode::RespectLimit,
                PriceTimeStrategy::ResidualMode::Rest) {}

void IcebergStrategy::execute(std::unique_ptr<Order> order, MatchingContext& context) {
    if (!order)
        return;

    if (!order->hasDisplayQuantity()) {
        order->setDisplayQuantity(order->remaining_quantity());
    }

    executor_.execute(std::move(order), context);
}
