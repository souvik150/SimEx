#pragma once

#include "strategies/MatchingStrategy.h"

class PriceTimeStrategy final : public MatchingStrategy {
public:
    enum class PriceMode { RespectLimit, IgnoreLimit };
    enum class ResidualMode { Rest, Cancel };

    explicit PriceTimeStrategy(PriceMode priceMode = PriceMode::RespectLimit,
                      ResidualMode residualMode = ResidualMode::Rest)
        : price_mode_(priceMode), residual_mode_(residualMode) {}

    void execute(std::unique_ptr<Order> order, MatchingContext& context) override;

private:
    PriceMode price_mode_;
    ResidualMode residual_mode_;
};
