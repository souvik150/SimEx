#pragma once

#include "strategies/MatchingStrategy.h"
#include "strategies/PriceTimeStrategy.h"

class FillOrKillStrategy final : public MatchingStrategy {
public:
    FillOrKillStrategy();
    void execute(std::unique_ptr<Order> order, MatchingContext& context) override;

private:
    PriceTimeStrategy executor_;
};
