//
// Created by souvik on 11/8/25.
//

#include "core/OrderBook.h"

#include "core/OrderBuilder.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

OrderBook::OrderBook() = default;

void OrderBook::addOrder(std::unique_ptr<Order> order) {
    Price px = order->price();
    Side side = order->side();
    OrderId orderId = order->orderId();

    if (side == Side::BUY) {
        auto bestAsk = asks_.best();
        if (bestAsk && px >= bestAsk->headOrder()->price()) {
            match(std::move(order), *bestAsk, Side::BUY);
            return;
        }

        if (auto* level = bids_.find(px); !level) {
            PriceLevel pl;
            pl.addOrder(std::move(order));
            bids_.insert(px, std::move(pl));
        } else {
            level->addOrder(std::move(order));
        }
        order_index_[orderId] = {Side::BUY, px};
    } else {
        auto bestBid = bids_.best();
        if (bestBid && px <= bestBid->headOrder()->price()) {
            match(std::move(order), *bestBid, Side::SELL);
            return;
        }

        auto* level = asks_.find(px);
        if (!level) {
            PriceLevel pl;
            pl.addOrder(std::move(order));
            asks_.insert(px, std::move(pl));
        } else {
            level->addOrder(std::move(order));
        }
        order_index_[orderId] = {Side::SELL, px};
    }
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
        std::cerr << "Modify failed: order not found\n";
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

                auto newOrder = OrderBuilder()
                    .setOrderId(oldOrder->orderId())
                    .setSide(oldOrder->side())
                    .setPrice(newPrice)
                    .setQuantity(oldOrder->pending_quantity())
                    .setTimestamp(std::chrono::high_resolution_clock::now())
                    .build();

                addOrder(std::move(newOrder));
                order_index_[orderId] = {Side::BUY, newPrice};
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

                auto newOrder = OrderBuilder()
                    .setOrderId(oldOrder->orderId())
                    .setSide(oldOrder->side())
                    .setPrice(newPrice)
                    .setQuantity(oldOrder->pending_quantity())
                    .setTimestamp(std::chrono::high_resolution_clock::now())
                    .build();

                addOrder(std::move(newOrder));
                order_index_[orderId] = {Side::SELL, newPrice};
                return;
            }

            default:
                return;
        }
    }
}

void OrderBook::match(std::unique_ptr<Order> incoming, PriceLevel& oppositeLevel, Side incomingSide) {
    while (!oppositeLevel.empty() && incoming->pending_quantity() > 0) {
        auto headOrder = oppositeLevel.headOrder();
        if (!headOrder) break;

        bool matchPossible = (incomingSide == Side::BUY)
            ? incoming->price() >= headOrder->price()
            : incoming->price() <= headOrder->price();

        if (!matchPossible) break;

        Qty tradeQty = std::min(incoming->pending_quantity(), headOrder->pending_quantity());
        Price tradePrice = headOrder->price();

        std::cout << "TRADE: "
                  << (incomingSide == Side::BUY ? "BUY" : "SELL")
                  << " matched with "
                  << (incomingSide == Side::BUY ? "SELL" : "BUY")
                  << " @ " << tradePrice
                  << " for " << tradeQty << " qty\n";

        incoming->addFill(tradeQty);
        oppositeLevel.addFill(tradeQty);

        if (PriceLevel* sameSideLevel =
        (incomingSide == Side::BUY) ? this->bids_.find(incoming->price())
                                    : this->asks_.find(incoming->price())) {
            sameSideLevel->decOpenQty(tradeQty);
                                    }

        if (headOrder->pending_quantity() == 0) {
            oppositeLevel.removeOrder(headOrder->orderId());
            if (oppositeLevel.empty()) {
                if (incomingSide == Side::BUY)
                    this->asks_.erase(tradePrice);
                else
                    this->bids_.erase(tradePrice);
                break;
            }
            continue;
        }

        if (incoming->pending_quantity() == 0)
            break;
    }

    if (incoming->pending_quantity() > 0) {
        if (incomingSide == Side::BUY) {
            if (auto* lvl = this->bids_.find(incoming->price()); !lvl) {
                PriceLevel pl;
                pl.addOrder(std::move(incoming));
                this->bids_.insert(pl.headOrder()->price(), std::move(pl));
            } else {
                lvl->addOrder(std::move(incoming));
            }
        } else {
            if (auto* lvl = this->asks_.find(incoming->price()); !lvl) {
                PriceLevel pl;
                pl.addOrder(std::move(incoming));
                this->asks_.insert(pl.headOrder()->price(), std::move(pl));
            } else {
                lvl->addOrder(std::move(incoming));
            }
        }
    }
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

    // Collect asks ascending (lowest first)
    asks_.inOrder([&](const Price& p, PriceLevel& lvl) {
        asks.emplace_back(p, &lvl);
    });

    // Collect bids ascending, but we want descending later
    bids_.inOrder([&](const Price& p, PriceLevel& lvl) {
        bids.emplace_back(p, &lvl);
    });


    const size_t rows = std::max(asks.size(), bids.size());

    std::cout << "\n" COLOR_BOLD
              << "╔══════════════════════════════════════════════════════════════════╗\n"
              << "║                           ORDER BOOK                             ║\n"
              << "╚══════════════════════════════════════════════════════════════════╝\n"
              << COLOR_RESET;

    std::cout << COLOR_DIM
              << std::setw(PRICE_WIDTH + QTY_WIDTH + 4) << "--- BIDS (BUY) ---"
              << std::setw(COL_GAP) << " "
              << std::setw(PRICE_WIDTH + QTY_WIDTH + 4) << "--- ASKS (SELL) ---"
              << COLOR_RESET << "\n";

    std::cout << COLOR_DIM
              << std::setw(PRICE_WIDTH) << "Price"
              << std::setw(QTY_WIDTH)   << "Qty"
              << std::setw(COL_GAP + 2) << " "
              << std::setw(PRICE_WIDTH) << "Price"
              << std::setw(QTY_WIDTH)   << "Qty"
              << COLOR_RESET << "\n";

    std::cout << COLOR_DIM
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

        std::cout << left.str()
                  << std::setw(COL_GAP) << " "
                  << right.str() << "\n";
    }

    std::cout << COLOR_DIM
              << "────────────────────────────────────────────────────────────────────\n"
              << COLOR_RESET;
}



HrtTime OrderBook::getTimestamp() {
    return std::chrono::high_resolution_clock::now();
}
