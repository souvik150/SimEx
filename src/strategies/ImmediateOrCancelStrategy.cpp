#include "strategies/ImmediateOrCancelStrategy.h"

#include "core/Order.h"

ImmediateOrCancelStrategy::ImmediateOrCancelStrategy()
    : executor_(PriceTimeStrategy::PriceMode::RespectLimit,
                PriceTimeStrategy::ResidualMode::Cancel) {}

void ImmediateOrCancelStrategy::execute(std::unique_ptr<Order> order, MatchingContext& context) {
    executor_.execute(std::move(order), context);
}
