#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "core/OrderBook.h"
#include "core/OrderBookManager.h"
#include "core/OrderBuilder.h"
#include "utils/LogMacros.h"

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::unique_ptr<Order> makeOrder(OrderId id, Side side, Price price, Qty qty,
                                 OrderType type = OrderType::LIMIT, Qty display = 0,
                                 InstrumentToken token = 1) {
    auto builder = OrderBuilder()
        .setOrderId(id)
        .setInstrumentToken(token)
        .setSide(side)
        .setPrice(price)
        .setQuantity(qty)
        .setTimestamp(std::chrono::high_resolution_clock::now())
        .setOrderType(type);

    if (display > 0) {
        builder.setDisplayQuantity(display);
    }

    return builder.build();
}
}

int main() {
    try {
        {
            OrderBook book;

            book.addOrder(makeOrder(1, Side::BUY, 1000, 10));
            expect(book.totalOpenQtyAt(Side::BUY, 1000) == 10, "First buy level open qty mismatch (expected 10)");

            book.addOrder(makeOrder(2, Side::BUY, 1000, 10));
            expect(book.totalOpenQtyAt(Side::BUY, 1000) == 20, "Aggregated buy qty should be 20");

            // Aggressive sell partially fills best buy level; remaining qty must reflect fills.
            book.addOrder(makeOrder(3, Side::SELL, 1000, 8));
            expect(book.totalOpenQtyAt(Side::BUY, 1000) == 12, "Partial fill did not reduce buy side depth correctly");
            expect(book.bestAsk() == nullptr, "Aggressive sell should not leave asks resting at 1000");

            // Another sell consumes the rest of the bids at 1000.
            book.addOrder(makeOrder(4, Side::SELL, 1000, 12));
            expect(book.totalOpenQtyAt(Side::BUY, 1000) == 0, "All buy liquidity at 1000 should be gone");
            expect(book.bestBid() == nullptr, "Best bid should be empty after full consumption");

            // Seed resting asks then send aggressive buys to ensure both sides update correctly.
            book.addOrder(makeOrder(5, Side::SELL, 1010, 7));
            expect(book.totalOpenQtyAt(Side::SELL, 1010) == 7, "Initial ask depth mismatch");

            book.addOrder(makeOrder(6, Side::BUY, 1010, 5));
            expect(book.totalOpenQtyAt(Side::SELL, 1010) == 2, "Ask depth after first aggressive buy incorrect");
            expect(book.totalOpenQtyAt(Side::BUY, 1010) == 0, "Aggressive buy should not rest when fully filled");

            book.addOrder(makeOrder(7, Side::BUY, 1010, 3));
            expect(book.totalOpenQtyAt(Side::SELL, 1010) == 0, "All asks at 1010 should be filled");
            expect(book.totalOpenQtyAt(Side::BUY, 1010) == 1, "Residual buy quantity should rest on the book");

            const Order* bestBid = book.bestBid();
            expect(bestBid != nullptr, "Best bid should exist");
            expect(bestBid->price() == 1010, "Best bid price mismatch");
            expect(bestBid->pending_quantity() == 1, "Best bid size mismatch");
        }

        {
            OrderBook book;
            // Two ask levels, one incoming buy sweeps both levels.
            book.addOrder(makeOrder(8, Side::SELL, 1000, 5));
            book.addOrder(makeOrder(9, Side::SELL, 1005, 7));

            book.addOrder(makeOrder(10, Side::BUY, 1010, 12));
            expect(book.bestAsk() == nullptr, "Sweep should consume every ask level");
            expect(book.totalOpenQtyAt(Side::SELL, 1000) == 0, "Lower ask level should be empty after sweep");
            expect(book.totalOpenQtyAt(Side::SELL, 1005) == 0, "Second ask level should be empty after sweep");
            expect(book.bestBid() == nullptr, "No residual buy liquidity should rest after full sweep");
        }

        {
            OrderBook book;
            // Partially fill a buy order, then modify to higher price/qty and ensure resting qty honors newQty.
            book.addOrder(makeOrder(11, Side::BUY, 1000, 10));
            book.addOrder(makeOrder(12, Side::SELL, 1000, 4));
            expect(book.totalOpenQtyAt(Side::BUY, 1000) == 6, "Buy order should have 6 remaining after partial fill");

            book.modifyOrder(11, 1010, 12);
            expect(book.totalOpenQtyAt(Side::BUY, 1000) == 0, "Old price level should be empty after reprice");
            expect(book.totalOpenQtyAt(Side::BUY, 1010) == 8, "Resting qty should reflect newQty - filled");

            const Order* updatedBestBid = book.bestBid();
            expect(updatedBestBid != nullptr, "Modified order should rest at new price");
            expect(updatedBestBid->price() == 1010, "Modified order price mismatch");
            expect(updatedBestBid->pending_quantity() == 8, "Modified order pending qty mismatch");
        }

        {
            OrderBook book;
            book.addOrder(makeOrder(13, Side::BUY, 1000, 5));
            expect(book.cancelOrder(13), "Cancel should remove resting order");
            expect(!book.cancelOrder(13), "Cancelling twice should fail cleanly");
            expect(book.bestBid() == nullptr, "Book should be empty after cancel");
        }

        {
            OrderBook book;
            book.addOrder(makeOrder(20, Side::SELL, 1000, 5));
            book.addOrder(makeOrder(21, Side::SELL, 1010, 7));

            book.addOrder(makeOrder(22, Side::BUY, 0, 12, OrderType::MARKET));
            expect(book.bestAsk() == nullptr, "Market buy should consume all resting asks");
            expect(book.bestBid() == nullptr, "Market order should not rest residual qty");
        }

        {
            OrderBook book;
            // IOC order that executes immediately and cancels remainder.
            book.addOrder(makeOrder(30, Side::SELL, 1000, 5));
            book.addOrder(makeOrder(31, Side::SELL, 1002, 4));

            book.addOrder(makeOrder(32, Side::BUY, 1002, 6, OrderType::IOC));
            expect(book.totalOpenQtyAt(Side::SELL, 1000) == 0, "IOC should consume lower ask first");
            expect(book.totalOpenQtyAt(Side::SELL, 1002) == 3, "IOC remainder cancels, leaving partial ask");
            expect(book.bestBid() == nullptr, "IOC must not rest on the book");

            // IOC that is priced away cancels entirely.
            book.addOrder(makeOrder(33, Side::BUY, 998, 4, OrderType::IOC));
            expect(book.bestBid() == nullptr, "Away IOC cancels without adding depth");
            expect(book.totalOpenQtyAt(Side::SELL, 1002) == 3, "Book should be unchanged");
        }

        {
            OrderBook book;
            // FOK succeeds only when full size available.
            book.addOrder(makeOrder(40, Side::SELL, 1000, 5));
            book.addOrder(makeOrder(41, Side::SELL, 1005, 7));

            book.addOrder(makeOrder(42, Side::BUY, 1005, 12, OrderType::FOK));
            expect(book.bestAsk() == nullptr, "All liquidity should be consumed on successful FOK");
            expect(book.totalOpenQtyAt(Side::SELL, 1000) == 0, "Lower level fully consumed");
            expect(book.totalOpenQtyAt(Side::SELL, 1005) == 0, "Upper level fully consumed");

            book.addOrder(makeOrder(43, Side::SELL, 1010, 4));
            book.addOrder(makeOrder(44, Side::BUY, 1005, 10, OrderType::FOK));
            expect(book.totalOpenQtyAt(Side::SELL, 1010) == 4, "Insufficient liquidity should leave book untouched");
            const Order* remainingAsk = book.bestAsk();
            expect(remainingAsk && remainingAsk->price() == 1010, "Remaining ask should stay posted");
        }

        {
            OrderBook book;
            // Iceberg should refresh visible clip repeatedly until fully consumed.
            book.addOrder(makeOrder(50, Side::SELL, 1000, 12, OrderType::ICEBERG, 4));
            expect(book.totalOpenQtyAt(Side::SELL, 1000) == 4, "Only display qty should be exposed");

            book.addOrder(makeOrder(51, Side::BUY, 1000, 4));
            expect(book.totalOpenQtyAt(Side::SELL, 1000) == 4, "After first clip fills, iceberg refreshes");

            book.addOrder(makeOrder(52, Side::BUY, 1000, 4));
            expect(book.totalOpenQtyAt(Side::SELL, 1000) == 4, "Refresh again for remaining hidden qty");

            book.addOrder(makeOrder(53, Side::BUY, 1000, 4));
            expect(book.totalOpenQtyAt(Side::SELL, 1000) == 0, "All liquidity gone after final clip");
        }

        {
            OrderBookManager manager;
            const InstrumentToken nifty = 111;
            const InstrumentToken bank = 222;

            manager.addOrder(makeOrder(60, Side::BUY, 1000, 5, OrderType::LIMIT, 0, nifty));
            manager.addOrder(makeOrder(61, Side::BUY, 1000, 7, OrderType::LIMIT, 0, bank));

            expect(manager.totalOpenQtyAt(nifty, Side::BUY, 1000) == 5, "Token 111 depth mismatch");
            expect(manager.totalOpenQtyAt(bank, Side::BUY, 1000) == 7, "Token 222 depth mismatch");

            manager.addOrder(makeOrder(62, Side::SELL, 1000, 5, OrderType::LIMIT, 0, nifty));
            expect(manager.totalOpenQtyAt(nifty, Side::BUY, 1000) == 0, "Matched depth should vanish for token 111");
            expect(manager.totalOpenQtyAt(bank, Side::BUY, 1000) == 7, "Other instrument depth must remain untouched");

            const Order* bankBid = manager.bestBid(bank);
            expect(bankBid && bankBid->pending_quantity() == 7, "Token 222 best bid size mismatch");

            manager.addOrder(makeOrder(63, Side::SELL, 1000, 7, OrderType::LIMIT, 0, bank));
            expect(manager.bestBid(bank) == nullptr, "All bank orders should fill out");
        }

        return 0;
    } catch (const std::exception& ex) {
        LOG_ERROR("OrderBook tests failed: {}", ex.what());
        return 1;
    }
}
