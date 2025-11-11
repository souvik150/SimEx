//
// Created by souvik on 11/8/25.
//

#include <chrono>

#include "core/OrderBookManager.h"
#include "core/OrderBuilder.h"
#include "types/OrderType.h"
#include "utils/Logger.h"

int main() {
    try {
        OrderBookManager books;
        auto timeCursor = std::chrono::high_resolution_clock::now();
        OrderId nextId = 1;

        constexpr InstrumentToken niftyToken = 26000;
        constexpr InstrumentToken bankToken = 35000;

        auto submitOrder = [&](InstrumentToken token, Side side, Price price, Qty qty, OrderType type, Qty display = 0) -> OrderId {
            OrderBuilder builder;
            builder.setOrderId(nextId++)
                   .setInstrumentToken(token)
                   .setSide(side)
                   .setPrice(price)
                   .setQuantity(qty)
                   .setOrderType(type)
                   .setTimestamp(timeCursor);
            if (display > 0)
                builder.setDisplayQuantity(display);

            auto order = builder.build();
            logging::logger().info("→ new order for token {}", token);
            order->print();
            books.addOrder(std::move(order));
            if (auto* book = books.findBook(token)) {
                logging::logger().info("Current book for token {}", token);
                book->printBook();
            }
            timeCursor += std::chrono::nanoseconds(1);
            return nextId - 1;
        };

        auto submitNifty = [&](Side side, Price price, Qty qty, OrderType type, Qty display = 0) {
            return submitOrder(niftyToken, side, price, qty, type, display);
        };

        logging::logger().info("\nSeeding resting limit liquidity");
        submitNifty(Side::BUY, 1000, 8, OrderType::LIMIT);
        submitNifty(Side::BUY, 995, 6, OrderType::LIMIT);
        submitNifty(Side::SELL, 1005, 7, OrderType::LIMIT);
        submitNifty(Side::SELL, 1010, 5, OrderType::LIMIT);

        logging::logger().info("\nMarket order sweeps asks");
        submitNifty(Side::BUY, 0, 12, OrderType::MARKET);

        logging::logger().info("\nRebuilding asks for IOC demo");
        submitNifty(Side::SELL, 1004, 4, OrderType::LIMIT);
        submitNifty(Side::SELL, 1006, 5, OrderType::LIMIT);

        logging::logger().info("\nIOC order matches depth and cancels remainder");
        submitNifty(Side::BUY, 1006, 6, OrderType::IOC);
        logging::logger().info("\nIOC priced away cancels immediately");
        submitNifty(Side::BUY, 1000, 4, OrderType::IOC);

        logging::logger().info("\nPreparing depth for FOK scenario");
        submitNifty(Side::SELL, 1005, 2, OrderType::LIMIT);
        submitNifty(Side::SELL, 1005, 2, OrderType::LIMIT);

        logging::logger().info("\nFOK succeeds when full size is available");
        submitNifty(Side::BUY, 1006, 7, OrderType::FOK);

        logging::logger().info("\nFOK fails when liquidity is insufficient");
        submitNifty(Side::SELL, 1008, 3, OrderType::LIMIT);
        submitNifty(Side::BUY, 1006, 5, OrderType::FOK);

        logging::logger().info("\nIceberg order exposes clips and refreshes");
        submitNifty(Side::SELL, 1002, 12, OrderType::ICEBERG, 4);
        submitNifty(Side::BUY, 1002, 4, OrderType::LIMIT);
        submitNifty(Side::BUY, 1002, 4, OrderType::LIMIT);
        submitNifty(Side::BUY, 1002, 4, OrderType::LIMIT);

        logging::logger().info("\n✏Demonstrating modify & cancel on resting orders");
        const OrderId modTarget = submitNifty(Side::BUY, 998, 6, OrderType::LIMIT);
        logging::logger().info("Modifying order {} -> px=1001 qty=9", modTarget);
        books.modifyOrder(niftyToken, modTarget, 1001, 9);
        if (auto* book = books.findBook(niftyToken)) {
            book->printBook();
        }

        const OrderId cancelTarget = submitNifty(Side::SELL, 1009, 5, OrderType::LIMIT);
        logging::logger().info("Cancelling order {}", cancelTarget);
        if (books.cancelOrder(niftyToken, cancelTarget))
            logging::logger().info("Order {} cancelled", cancelTarget);
        else
            logging::logger().warn("Failed to cancel order {}", cancelTarget);
        if (auto* book = books.findBook(niftyToken)) {
            book->printBook();
        }

        logging::logger().info("\nAdding flows for a second instrument");
        submitOrder(bankToken, Side::BUY, 2050, 10, OrderType::LIMIT);
        submitOrder(bankToken, Side::SELL, 2055, 7, OrderType::LIMIT);
        submitOrder(bankToken, Side::BUY, 2055, 4, OrderType::LIMIT);

        logging::logger().info("\n📘 Final book state:");
        if (auto* book = books.findBook(niftyToken)) {
            logging::logger().info("Nifty book:");
            book->printBook();
        }
        if (auto* book = books.findBook(bankToken)) {
            logging::logger().info("Bank book:");
            book->printBook();
        }
    } catch (const std::exception& ex) {
        logging::logger().error("Unhandled exception: {}", ex.what());
        return 1;
    }

    return 0;
}
