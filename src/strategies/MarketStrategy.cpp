#include "strategies/MarketStrategy.h"

#include "core/Order.h"

MarketStrategy::MarketStrategy()
    : executor_(PriceTimeStrategy::PriceMode::IgnoreLimit,
                PriceTimeStrategy::ResidualMode::Cancel) {}

void MarketStrategy::execute(std::unique_ptr<Order> order, MatchingContext& context) {
    executor_.execute(std::move(order), context);
}
