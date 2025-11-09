#pragma once

#include "strategies/MatchingStrategy.h"
#include "strategies/PriceTimeStrategy.h"

class ImmediateOrCancelStrategy final : public MatchingStrategy {
public:
    ImmediateOrCancelStrategy();
    void execute(std::unique_ptr<Order> order, MatchingContext& context) override;

private:
    PriceTimeStrategy executor_;
};
