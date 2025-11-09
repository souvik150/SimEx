#pragma once

#include "strategies/MatchingStrategy.h"
#include "strategies/PriceTimeStrategy.h"

class IcebergStrategy final : public MatchingStrategy {
public:
    IcebergStrategy();
    void execute(std::unique_ptr<Order> order, MatchingContext& context) override;

private:
    PriceTimeStrategy executor_;
};
