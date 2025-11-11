//
// Created by souvik on 11/8/25.
//

#include "core/OrderBook.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "strategies/FillOrKillStrategy.h"
#include "strategies/IcebergStrategy.h"
#include "strategies/ImmediateOrCancelStrategy.h"
#include "strategies/MarketStrategy.h"
#include "strategies/PriceTimeStrategy.h"
#include "utils/Logger.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

OrderBook::OrderBook()
    : node_pool_(),
      context_(*this) {
    strategies_[OrderType::LIMIT] = std::make_unique<PriceTimeStrategy>();
    strategies_[OrderType::MARKET] = std::make_unique<MarketStrategy>();
    strategies_[OrderType::IOC] = std::make_unique<ImmediateOrCancelStrategy>();
    strategies_[OrderType::FOK] = std::make_unique<FillOrKillStrategy>();
    strategies_[OrderType::ICEBERG] = std::make_unique<IcebergStrategy>();

    trade_listener_ = [](const TradeEvent& event) {
        logging::logger().info(
            "TRADE: {} matched with {} @ {} for {} qty",
            (event.aggressorSide == Side::BUY ? "BUY" : "SELL"),
            (event.restingSide == Side::BUY ? "BUY" : "SELL"),
            event.price,
            event.quantity);
    };
}

void OrderBook::addOrder(std::unique_ptr<Order> order) {
    if (!order)
        return;

    if (auto* strategy = strategyFor(order->type())) {
        strategy->execute(std::move(order), context_);
    }
}

void OrderBook::setMatchingStrategy(OrderType type, std::unique_ptr<MatchingStrategy> strategy) {
    if (strategy) {
        strategies_[type] = std::move(strategy);
    }
}

void OrderBook::setTradeListener(TradeListener listener) {
    trade_listener_ = std::move(listener);
}

bool OrderBook::cancelOrder(uint64_t orderId) {
    auto it = order_index_.find(orderId);
    if (it == order_index_.end()) return false;

    const auto& ref = it->second;
    if (ref.side == Side::BUY) {
        auto* level = bids_.find(ref.price);
        if (level && level->removeOrder(orderId)) {
            if (level->empty()) bids_.erase(ref.price);
            order_index_.erase(it);
            return true;
        }
    } else {
        auto* level = asks_.find(ref.price);
        if (level && level->removeOrder(orderId)) {
            if (level->empty()) asks_.erase(ref.price);
            order_index_.erase(it);
            return true;
        }
    }
    return false;
}

void OrderBook::modifyOrder(OrderId orderId, Price newPrice, Qty newQty) {
    auto it = order_index_.find(orderId);
    if (it == order_index_.end()) {
        logging::logger().warn("Modify failed: order {} not found", orderId);
        return;
    }

    const auto& ref = it->second;

    if (ref.side == Side::BUY) {
        auto* level = bids_.find(ref.price);
        if (!level) return;

        auto result = level->modifyOrder(orderId, newPrice, newQty);
        switch (result) {
            case ModifyInPlace:
                return;

            case NeedsReinsert: {
                auto oldOrder = level->removeOrder(orderId);
                if (!oldOrder) return;

                if (level->empty())
                    bids_.erase(ref.price);
                order_index_.erase(orderId);

                if (newQty != oldOrder->quantity())
                    oldOrder->modifyQty(newQty);
                if (newPrice != oldOrder->price())
                    oldOrder->modifyPrice(newPrice);

                addOrder(std::move(oldOrder));
                return;
            }

            default:
                return;
        }
    }
    else {
        auto* level = asks_.find(ref.price);
        if (!level) return;

        auto result = level->modifyOrder(orderId, newPrice, newQty);
        switch (result) {
            case ModifyInPlace:
                return;

            case NeedsReinsert: {
                auto oldOrder = level->removeOrder(orderId);
                if (!oldOrder) return;

                if (level->empty())
                    asks_.erase(ref.price);
                order_index_.erase(orderId);

                if (newQty != oldOrder->quantity())
                    oldOrder->modifyQty(newQty);
                if (newPrice != oldOrder->price())
                    oldOrder->modifyPrice(newPrice);

                addOrder(std::move(oldOrder));
                return;
            }

            default:
                return;
        }
    }
}

PriceLevel* OrderBook::BookContext::bestLevel(Side side) {
    if (side == Side::BUY)
        return book_.bids_.best();
    if (side == Side::SELL)
        return book_.asks_.best();
    return nullptr;
}

void OrderBook::BookContext::restOrder(std::unique_ptr<Order> order) {
    if (!order)
        return;

    const Side side = order->side();
    const Price price = order->price();
    const OrderId orderId = order->orderId();

    order->refreshWorkingQuantity();

    if (PriceLevel* level = findLevel(side, price); level) {
        level->addOrder(std::move(order));
    } else {
        PriceLevel pl(&book_.node_pool_);
        pl.addOrder(std::move(order));
        insertLevel(side, price, std::move(pl));
    }

    book_.order_index_[orderId] = {side, price};
}

void OrderBook::BookContext::removeRestingOrder(Side restingSide, Price price, PriceLevel& level, OrderId orderId) {
    auto removed = level.removeOrder(orderId);
    book_.order_index_.erase(orderId);

    if (removed && removed->hasDisplayQuantity() && removed->remaining_quantity() > 0) {
        removed->refreshWorkingQuantity();
        restOrder(std::move(removed));
        return;
    }

    if (level.empty()) {
        if (restingSide == Side::BUY)
            book_.bids_.erase(price);
        else if (restingSide == Side::SELL)
            book_.asks_.erase(price);
    }
}

void OrderBook::BookContext::recordTrade(const TradeEvent& event) {
    book_.emitTrade(event);
}

PriceLevel* OrderBook::BookContext::findLevel(Side side, Price price) {
    if (side == Side::BUY)
        return book_.bids_.find(price);
    if (side == Side::SELL)
        return book_.asks_.find(price);
    return nullptr;
}

void OrderBook::BookContext::insertLevel(Side side, Price price, PriceLevel&& level) {
    if (side == Side::BUY)
        book_.bids_.insert(price, std::move(level));
    else if (side == Side::SELL)
        book_.asks_.insert(price, std::move(level));
}

Qty OrderBook::BookContext::availableLiquidityAgainst(Side incomingSide, Price limitPrice) const {
    if (incomingSide == Side::BUY)
        return liquidityForBuy(limitPrice);
    if (incomingSide == Side::SELL)
        return liquidityForSell(limitPrice);
    return 0;
}

Qty OrderBook::BookContext::liquidityForBuy(Price limitPrice) const {
    Qty total = 0;
    book_.asks_.inOrder([&](const Price& price, PriceLevel& level) {
        if (price <= limitPrice)
            total += level.openQty();
    });
    return total;
}

Qty OrderBook::BookContext::liquidityForSell(Price limitPrice) const {
    Qty total = 0;
    book_.bids_.inOrder([&](const Price& price, PriceLevel& level) {
        if (price >= limitPrice)
            total += level.openQty();
    });
    return total;
}

void OrderBook::emitTrade(const TradeEvent& event) const {
    if (trade_listener_)
        trade_listener_(event);
}

MatchingStrategy* OrderBook::strategyFor(OrderType type) {
    auto it = strategies_.find(type);
    if (it != strategies_.end() && it->second)
        return it->second.get();

    auto fallback = strategies_.find(OrderType::LIMIT);
    return (fallback != strategies_.end()) ? fallback->second.get() : nullptr;
}

const Order* OrderBook::bestBid() const {
    auto* best = bids_.best();
    return best ? best->headOrder() : nullptr;
}

const Order* OrderBook::bestAsk() const {
    auto* best = asks_.best();
    return best ? best->headOrder() : nullptr;
}

Qty OrderBook::totalOpenQtyAt(Side side, Price price) const {
    const PriceLevel* level = (side == Side::BUY)
        ? bids_.find(price)
        : asks_.find(price);
    return level ? level->openQty() : 0;
}

void OrderBook::printBook() const {
    constexpr int PRICE_WIDTH = 10;
    constexpr int QTY_WIDTH   = 8;
    constexpr int COL_GAP     = 6;

    std::vector<std::pair<Price, PriceLevel*>> asks, bids;

    asks_.inOrder([&](const Price& p, PriceLevel& lvl) {
        asks.emplace_back(p, &lvl);
    });


    bids_.inOrder([&](const Price& p, PriceLevel& lvl) {
        bids.emplace_back(p, &lvl);
    });


    const size_t rows = std::max(asks.size(), bids.size());

    std::ostringstream out;
    out << "\n" COLOR_BOLD
        << "╔══════════════════════════════════════════════════════════════════╗\n"
        << "║                           ORDER BOOK                             ║\n"
        << "╚══════════════════════════════════════════════════════════════════╝\n"
        << COLOR_RESET;

    out << COLOR_DIM
        << std::setw(PRICE_WIDTH + QTY_WIDTH + 4) << "--- BIDS (BUY) ---"
        << std::setw(COL_GAP) << " "
        << std::setw(PRICE_WIDTH + QTY_WIDTH + 4) << "--- ASKS (SELL) ---"
        << COLOR_RESET << "\n";

    out << COLOR_DIM
        << std::setw(PRICE_WIDTH) << "Price"
        << std::setw(QTY_WIDTH)   << "Qty"
        << std::setw(COL_GAP + 2) << " "
        << std::setw(PRICE_WIDTH) << "Price"
        << std::setw(QTY_WIDTH)   << "Qty"
        << COLOR_RESET << "\n";

    out << COLOR_DIM
        << "────────────────────────────────────────────────────────────────────\n"
        << COLOR_RESET;

    for (size_t i = 0; i < rows; ++i) {
        std::ostringstream left, right;

        bool isBestBid = (i == 0 && !bids.empty());
        bool isBestAsk = (i == 0 && !asks.empty());

        // --- BID (BUY) ---
        if (i < bids.size()) {
            Qty pending_qty = bids[i].second ? bids[i].second->openQty() : 0;
            left << (isBestBid ? COLOR_BOLD COLOR_GREEN : COLOR_GREEN)
                 << std::fixed << std::setprecision(2)
                 << std::setw(PRICE_WIDTH) << bids[i].first
                 << std::setw(QTY_WIDTH)   << pending_qty
                 << (isBestBid ? "  ←" : "")
                 << COLOR_RESET;
        } else {
            left << std::string(PRICE_WIDTH + QTY_WIDTH + 3, ' ');
        }

        // --- ASK (SELL) ---
        if (i < asks.size()) {
            Qty pending_qty = asks[i].second ? asks[i].second->openQty() : 0;
            right << (isBestAsk ? COLOR_BOLD COLOR_RED : COLOR_RED)
                  << std::fixed << std::setprecision(2)
                  << std::setw(PRICE_WIDTH) << asks[i].first
                  << std::setw(QTY_WIDTH)   << pending_qty
                  << (isBestAsk ? "  →" : "")
                  << COLOR_RESET;
        }

        out << left.str()
            << std::setw(COL_GAP) << " "
            << right.str() << "\n";
    }

    out << COLOR_DIM
        << "────────────────────────────────────────────────────────────────────\n"
        << COLOR_RESET;

    logging::logger().info(out.str());
}
