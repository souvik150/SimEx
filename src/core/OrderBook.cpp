#include "core/OrderBook.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
#include <vector>

#include "utils/Affinity.h"
#include "utils/LogMacros.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

OrderBook::OrderBook(bool use_std_map)
    : bids_(Side::BUY),
      asks_(Side::SELL),
      trade_ring_(2048),
      trade_thread_([this] { tradeWorker(); }) {
    (void)use_std_map;

    trade_listener_ = [](const TradeEvent& event) {
#if defined(ENABLE_INFO_LOGS)
        LOG_INFO(
            "TRADE: token={} {} matched with {} @ {} for {} qty",
            event.instrument,
            (event.aggressorSide == Side::BUY ? "BUY" : "SELL"),
            (event.restingSide == Side::BUY ? "BUY" : "SELL"),
            event.price,
            event.quantity);
#else
        (void)event;
#endif
    };
}

OrderBook::~OrderBook() {
    trade_running_.store(false, std::memory_order_release);
    trade_head_.store(UINT32_MAX, std::memory_order_release);
    if (trade_thread_.joinable()) {
        trade_thread_.join();
    }
}

void OrderBook::addOrder(std::unique_ptr<Order> order) {
    if (!order) {
        return;
    }

    const OrderId orderId = order->orderId();
    orders_.store(std::move(order));
    processOrder(orderId);
}

void OrderBook::processOrder(OrderId orderId) {
    Order& order = orders_.require(orderId);
    MatchParams params{};
    switch (order.type()) {
        case OrderType::LIMIT:
            params = {.respectPrice = true, .allowRest = true};
            executeMatch(orderId, params);
            break;
        case OrderType::MARKET:
            params = {.respectPrice = false, .allowRest = false};
            executeMatch(orderId, params);
            break;
        case OrderType::IOC:
            params = {.respectPrice = true, .allowRest = false};
            executeMatch(orderId, params);
            break;
        case OrderType::FOK:
            if (!ensureFokLiquidity(order)) {
                releaseOrderInternal(orderId);
                return;
            }
            params = {.respectPrice = true, .allowRest = false};
            executeMatch(orderId, params);
            break;
        case OrderType::ICEBERG:
            handleIceberg(order);
            params = {.respectPrice = true, .allowRest = true};
            executeMatch(orderId, params);
            break;
        default:
            releaseOrderInternal(orderId);
            break;
    }
}

void OrderBook::setTradeListener(TradeListener listener) {
    trade_listener_ = std::move(listener);
}

bool OrderBook::cancelOrder(OrderId orderId) {
    auto* ref = findOrderRef(orderId);
    if (!ref) {
        return false;
    }

    const OrderRef stored = *ref;
    PriceLevel* level = (stored.side == Side::BUY)
        ? bids_.findLevel(stored.price)
        : asks_.findLevel(stored.price);
    if (!level) {
        clearOrderRef(orderId);
        return false;
    }

    if (stored.slot == PriceLevel::kInvalidSlot) {
        clearOrderRef(orderId);
        return false;
    }

    if (!level->removeOrderAt(stored.slot, orderId, orders_)) {
        clearOrderRef(orderId);
        return false;
    }

    clearOrderRef(orderId);
    if (level->empty()) {
        if (stored.side == Side::BUY) {
            bids_.eraseLevel(stored.price);
        } else {
            asks_.eraseLevel(stored.price);
        }
    }

    orders_.erase(orderId);
    return true;
}

void OrderBook::modifyOrder(OrderId orderId, Price newPrice, Qty newQty) {
    auto* ref = findOrderRef(orderId);
    if (!ref) {
        LOG_WARN("Modify failed: order {} not found", orderId);
        return;
    }

    Order& order = orders_.require(orderId);
    const OrderRef stored = *ref;
    PriceLevel* level = (stored.side == Side::BUY)
        ? bids_.findLevel(stored.price)
        : asks_.findLevel(stored.price);
    if (!level) {
        LOG_WARN("Modify failed: level for order {} not found", orderId);
        return;
    }

    const bool priceChanged = newPrice != order.price();
    const bool qtyIncrease = newQty > order.quantity();

    if (!priceChanged && !qtyIncrease) {
        const Qty beforePending = order.pending_quantity();
        if (!order.modifyQty(newQty)) {
            LOG_WARN("Modify failed: invalid quantity {} for order {}", newQty, orderId);
            return;
        }
        const Qty afterPending = order.pending_quantity();
        if (afterPending < beforePending) {
            level->decOpenQty(beforePending - afterPending);
        }
        return;
    }

    if (!level->removeOrderAt(stored.slot, orderId, orders_)) {
        return;
    }
    clearOrderRef(orderId);
    if (level->empty()) {
        if (stored.side == Side::BUY) {
            bids_.eraseLevel(stored.price);
        } else {
            asks_.eraseLevel(stored.price);
        }
    }

    if (!order.modifyQty(newQty)) {
        LOG_WARN("Modify failed: invalid quantity {} for order {}", newQty, orderId);
        orders_.erase(orderId);
        return;
    }
    if (priceChanged) {
        order.modifyPrice(newPrice);
    }
    order.refreshWorkingQuantity();
    processOrder(orderId);
}

PriceLevel* OrderBook::bestLevelMutable(Side side) {
    return (side == Side::BUY) ? bids_.bestLevel() : asks_.bestLevel();
}

const PriceLevel* OrderBook::bestLevelMutable(Side side) const {
    return (side == Side::BUY) ? bids_.bestLevel() : asks_.bestLevel();
}

PriceLevel* OrderBook::findLevel(Side side, Price price) {
    return (side == Side::BUY) ? bids_.findLevel(price) : asks_.findLevel(price);
}

PriceLevel* OrderBook::ensureLevel(Side side, Price price) {
    return (side == Side::BUY) ? bids_.ensureLevel(price) : asks_.ensureLevel(price);
}

void OrderBook::eraseLevelIfEmpty(Side side, Price price, PriceLevel& level) {
    if (!level.empty()) {
        return;
    }
    if (side == Side::BUY) {
        bids_.eraseLevel(price);
    } else {
        asks_.eraseLevel(price);
    }
}

void OrderBook::restOrderInternal(OrderId orderId) {
    Order& order = orders_.require(orderId);
    order.refreshWorkingQuantity();
    PriceLevel* level = ensureLevel(order.side(), order.price());
    if (!level) {
        LOG_ERROR("Failed to allocate price level for order {}", orderId);
        releaseOrderInternal(orderId);
        return;
    }
    const bool wasEmpty = level->empty();
    const size_t slot = level->addOrder(orderId, orders_);
    if (slot == PriceLevel::kInvalidSlot) {
        LOG_ERROR("Failed to reserve slot for order {}", orderId);
        releaseOrderInternal(orderId);
        return;
    }
    ensureOrderIndexCapacity(orderId);
    order_index_[orderId] = {order.side(), order.price(), slot};
    if (wasEmpty) {
        if (order.side() == Side::BUY) {
            bids_.markLevelNonEmpty(order.price());
        } else if (order.side() == Side::SELL) {
            asks_.markLevelNonEmpty(order.price());
        }
    }
}

void OrderBook::removeRestingOrderInternal(Side restingSide, Price price, PriceLevel& level, OrderId orderId) {
    auto* ref = findOrderRef(orderId);
    const size_t slot = ref ? ref->slot : PriceLevel::kInvalidSlot;
    if (!level.removeOrderAt(slot, orderId, orders_)) {
        return;
    }
    clearOrderRef(orderId);
    eraseLevelIfEmpty(restingSide, price, level);

    Order& order = orders_.require(orderId);
    if (order.hasDisplayQuantity() && order.remaining_quantity() > 0) {
        order.refreshWorkingQuantity();
        restOrderInternal(orderId);
        return;
    }
    releaseOrderInternal(orderId);
}

void OrderBook::releaseOrderInternal(OrderId orderId) {
    clearOrderRef(orderId);
    orders_.erase(orderId);
}

Qty OrderBook::liquidityForBuy(Price limitPrice) const {
    Qty total = 0;
    asks_.forEachAscending([&](Price px, const PriceLevel& level) {
        if (px <= limitPrice) {
            total += level.openQty();
        }
    });
    return total;
}

Qty OrderBook::liquidityForSell(Price limitPrice) const {
    Qty total = 0;
    bids_.forEachAscending([&](Price px, const PriceLevel& level) {
        if (px >= limitPrice) {
            total += level.openQty();
        }
    });
    return total;
}

Qty OrderBook::availableLiquidityAgainst(Side incomingSide, Price limitPrice) const {
    if (incomingSide == Side::BUY) {
        return liquidityForBuy(limitPrice);
    }
    if (incomingSide == Side::SELL) {
        return liquidityForSell(limitPrice);
    }
    return 0;
}

void OrderBook::ensureOrderIndexCapacity(OrderId orderId) {
    const size_t required = static_cast<size_t>(orderId) + 1;
    if (required <= order_index_.size()) {
        return;
    }
    size_t newSize = ((required + kOrderIndexChunk - 1) / kOrderIndexChunk) * kOrderIndexChunk;
    order_index_.resize(newSize);
}

OrderBook::OrderRef* OrderBook::findOrderRef(OrderId orderId) {
    if (orderId >= order_index_.size()) {
        return nullptr;
    }
    auto& ref = order_index_[orderId];
    return (ref.side == Side::INVALID) ? nullptr : &ref;
}

const OrderBook::OrderRef* OrderBook::findOrderRef(OrderId orderId) const {
    if (orderId >= order_index_.size()) {
        return nullptr;
    }
    const auto& ref = order_index_[orderId];
    return (ref.side == Side::INVALID) ? nullptr : &ref;
}

void OrderBook::clearOrderRef(OrderId orderId) {
    if (orderId < order_index_.size()) {
        order_index_[orderId] = {};
    }
}

void OrderBook::handleIceberg(Order& order) {
    if (!order.hasDisplayQuantity()) {
        order.setDisplayQuantity(order.remaining_quantity());
    }
    order.refreshWorkingQuantity();
}

bool OrderBook::ensureFokLiquidity(const Order& order) const {
    const Price limit = order.price();
    const Qty required = order.pending_quantity();
    const Qty available = availableLiquidityAgainst(order.side(), limit);
    return available >= required;
}

void OrderBook::executeMatch(OrderId orderId, const MatchParams& params) {
    Order& order = orders_.require(orderId);
    const Side incomingSide = order.side();
    const Side oppositeSide = (incomingSide == Side::BUY) ? Side::SELL : Side::BUY;

    while (order.pending_quantity() > 0) {
        Price bestPrice = 0;
        PriceLevel* oppositeLevel = (oppositeSide == Side::BUY)
            ? bids_.bestLevel(bestPrice)
            : asks_.bestLevel(bestPrice);
        if (!oppositeLevel || oppositeLevel->empty()) {
            break;
        }

        if (params.respectPrice) {
            const bool matchPossible = (incomingSide == Side::BUY)
                ? order.price() >= bestPrice
                : order.price() <= bestPrice;
            if (!matchPossible) {
                break;
            }
        }

        const OrderId restingId = oppositeLevel->headOrderId();
        Order& headOrder = orders_.require(restingId);

        const Qty tradeQty = std::min(order.pending_quantity(), headOrder.pending_quantity());
        const Price tradePrice = headOrder.price();

        order.addFill(tradeQty);
        headOrder.addFill(tradeQty);
        oppositeLevel->decOpenQty(tradeQty);

        if (tradeQty > 0) {
            TradeEvent event{
                order.instrument_token(),
                incomingSide,
                order.orderId(),
                oppositeSide,
                restingId,
                tradePrice,
                tradeQty};
            dispatchTrade(event);
        }

        if (headOrder.pending_quantity() == 0) {
            removeRestingOrderInternal(oppositeSide, tradePrice, *oppositeLevel, restingId);
        }
    }

    if (params.allowRest && order.pending_quantity() > 0) {
        restOrderInternal(orderId);
    } else {
        releaseOrderInternal(orderId);
    }
}

void OrderBook::emitTrade(const TradeEvent& event) const {
    if (trade_listener_) {
        trade_listener_(event);
    }

    for (auto it = observers_.begin(); it != observers_.end();) {
        if (auto obs = it->lock()) {
            obs->onTrade(*this, event);
            ++it;
        } else {
            it = observers_.erase(it);
        }
    }
}

const Order* OrderBook::bestBid() const {
    const PriceLevel* level = bids_.bestLevel();
    if (!level) {
        return nullptr;
    }
    const OrderId headId = level->headOrderId();
    return orders_.find(headId);
}

const Order* OrderBook::bestAsk() const {
    const PriceLevel* level = asks_.bestLevel();
    if (!level) {
        return nullptr;
    }
    const OrderId headId = level->headOrderId();
    return orders_.find(headId);
}

Qty OrderBook::totalOpenQtyAt(Side side, Price price) const {
    return (side == Side::BUY)
        ? bids_.totalOpenQtyAt(price)
        : asks_.totalOpenQtyAt(price);
}

void OrderBook::printBook() const {
    constexpr int PRICE_WIDTH = 10;
    constexpr int QTY_WIDTH   = 8;
    constexpr int COL_GAP     = 6;

    std::vector<std::pair<Price, const PriceLevel*>> asks;
    std::vector<std::pair<Price, const PriceLevel*>> bids;

    asks_.forEachAscending([&](Price price, const PriceLevel& level) {
        asks.emplace_back(price, &level);
    });
    bids_.forEachAscending([&](Price price, const PriceLevel& level) {
        bids.emplace_back(price, &level);
    });

    std::sort(asks.begin(), asks.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });
    std::sort(bids.begin(), bids.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first > rhs.first;
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

        const bool isBestBid = (i == 0 && !bids.empty());
        const bool isBestAsk = (i == 0 && !asks.empty());

        if (i < bids.size()) {
            const Qty pending_qty = bids[i].second ? bids[i].second->openQty() : 0;
            left << (isBestBid ? COLOR_BOLD COLOR_GREEN : COLOR_GREEN)
                 << std::fixed << std::setprecision(2)
                 << std::setw(PRICE_WIDTH) << bids[i].first
                 << std::setw(QTY_WIDTH)   << pending_qty
                 << (isBestBid ? "  ←" : "")
                 << COLOR_RESET;
        } else {
            left << std::string(PRICE_WIDTH + QTY_WIDTH + 3, ' ');
        }

        if (i < asks.size()) {
            const Qty pending_qty = asks[i].second ? asks[i].second->openQty() : 0;
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

    LOG_INFO("{}", out.str());
}

void OrderBook::tradeWorker() {
    const uint32_t mask = static_cast<uint32_t>(trade_ring_.size() - 1);
    while (trade_running_.load(std::memory_order_acquire) ||
           trade_tail_.load(std::memory_order_acquire) != trade_head_.load(std::memory_order_acquire)) {
        const uint32_t tail = trade_tail_.load(std::memory_order_acquire);
        const uint32_t head = trade_head_.load(std::memory_order_acquire);
        if (tail == head) {
            std::this_thread::yield();
            continue;
        }
        TradeEvent event = trade_ring_[tail & mask];
        trade_tail_.store(tail + 1, std::memory_order_release);
        emitTrade(event);
    }
}

void OrderBook::dispatchTrade(const TradeEvent& event) {
    last_trade_price_.store(event.price, std::memory_order_relaxed);
    last_trade_qty_.store(event.quantity, std::memory_order_relaxed);
    const uint32_t mask = static_cast<uint32_t>(trade_ring_.size() - 1);
    while (true) {
        const uint32_t tail = trade_tail_.load(std::memory_order_acquire);
        const uint32_t head = trade_head_.load(std::memory_order_relaxed);
        if (head - tail >= trade_ring_.size()) {
            trade_tail_.store(tail + 1, std::memory_order_release);
            continue;
        }
        trade_ring_[head & mask] = event;
        trade_head_.store(head + 1, std::memory_order_release);
        break;
    }
}

void OrderBook::setInstrumentToken(InstrumentToken token) {
    instrument_token_ = token;
}

InstrumentToken OrderBook::instrument_token() const {
    return instrument_token_;
}

void OrderBook::addObserver(const std::shared_ptr<OrderBookObserver>& observer) {
    if (!observer) {
        return;
    }
    observers_.push_back(observer);
}

void OrderBook::snapshot(std::vector<std::pair<Price, Qty>>& bids, std::vector<std::pair<Price, Qty>>& asks) const {
    bids.clear();
    asks.clear();

    bids_.forEachAscending([&](Price price, const PriceLevel& level) {
        bids.emplace_back(price, level.openQty());
    });
    asks_.forEachAscending([&](Price price, const PriceLevel& level) {
        asks.emplace_back(price, level.openQty());
    });
}

Price OrderBook::last_trade_price() const {
    return last_trade_price_.load(std::memory_order_relaxed);
}

Qty OrderBook::last_trade_quantity() const {
    return last_trade_qty_.load(std::memory_order_relaxed);
}

void OrderBook::bindTradeThreadToCores(const std::vector<int>& cores) {
    if (cores.empty() || !trade_thread_.joinable()) {
        return;
    }
    cpu::setThreadAffinity(trade_thread_, cores);
}
