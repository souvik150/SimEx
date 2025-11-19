#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include "core/OrderBook.h"
#include "core/OrderBuilder.h"

namespace {

constexpr InstrumentToken kInstrument = 26000;
constexpr Price kBuyBase = 1500;
constexpr Price kSellBase = 1520;

std::unique_ptr<Order> makeOrder(OrderId id, Side side, Price price, Qty qty) {
    OrderBuilder builder;
    builder.setOrderId(id)
        .setInstrumentToken(kInstrument)
        .setSide(side)
        .setPrice(price)
        .setQuantity(qty)
        .setOrderType(OrderType::LIMIT)
        .setTimestamp(std::chrono::high_resolution_clock::now());
    return builder.build();
}

struct BenchResult {
    uint64_t samples = 0;
    uint64_t total_ns = 0;
    uint64_t min_ns = std::numeric_limits<uint64_t>::max();
    uint64_t max_ns = 0;
    std::vector<uint64_t> measurements;

    void record(uint64_t ns) {
        ++samples;
        total_ns += ns;
        if (ns < min_ns) {
            min_ns = ns;
        }
        if (ns > max_ns) {
            max_ns = ns;
        }
        measurements.push_back(ns);
    }

    uint64_t percentile(double pct) const {
        if (measurements.empty()) {
            return 0;
        }
        auto values = measurements;
        const size_t rank = static_cast<size_t>(
            std::clamp(pct, 0.0, 1.0) * static_cast<double>(values.size() - 1));
        std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(rank), values.end());
        return values[rank];
    }
};

}  // namespace

int main() {
    OrderBook book(false);
    book.setInstrumentToken(kInstrument);
    book.setTradeListener([](const TradeEvent&) {});

    constexpr size_t kWarmup = 10'000;
    constexpr size_t kSamples = 100'000;

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> qtyDist(10, 200);

    auto run = [&](size_t iterations, bool measure) {
        BenchResult result;
        for (size_t i = 0; i < iterations; ++i) {
            const Side side = (i & 1) ? Side::BUY : Side::SELL;
            const Price price = (side == Side::BUY ? kBuyBase : kSellBase) + static_cast<Price>(i % 8);
            auto order = makeOrder(static_cast<OrderId>(i + 1), side, price, static_cast<Qty>(qtyDist(rng)));

            const auto start = std::chrono::steady_clock::now();
            book.addOrder(std::move(order));
            const auto end = std::chrono::steady_clock::now();

            if (measure) {
                const uint64_t ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
                result.record(ns);
            }
        }
        return result;
    };

    run(kWarmup, false);
    auto stats = run(kSamples, true);

    if (stats.samples == 0) {
        std::cout << "No samples recorded\n";
        return 0;
    }

    const double avg = static_cast<double>(stats.total_ns) / static_cast<double>(stats.samples);
    const auto p95 = stats.percentile(0.95);
    std::cout << "OrderBook::addOrder benchmark (" << stats.samples << " samples)\n"
              << "  avg: " << avg << " ns\n"
              << "  min: " << stats.min_ns << " ns\n"
              << "  max: " << stats.max_ns << " ns\n"
              << "  p95: " << p95 << " ns\n";
    return 0;
}
