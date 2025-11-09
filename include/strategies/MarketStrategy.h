#pragma once

#include "strategies/MatchingStrategy.h"
#include "strategies/PriceTimeStrategy.h"

class MarketStrategy final : public MatchingStrategy {
public:
    MarketStrategy();
    void execute(std::unique_ptr<Order> order, MatchingContext& context) override;

private:
    PriceTimeStrategy executor_;
};
