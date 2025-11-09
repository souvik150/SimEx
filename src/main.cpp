//
// Created by souvik on 11/8/25.
//

#include <chrono>

#include "core/OrderBook.h"
#include "core/OrderBuilder.h"
#include "types/OrderType.h"
#include "utils/Logger.h"

int main() {
    try {
        OrderBook book;
        auto timeCursor = std::chrono::high_resolution_clock::now();
        OrderId nextId = 1;

        auto submitOrder = [&](Side side, Price price, Qty qty, OrderType type, Qty display = 0) -> OrderId {
            OrderBuilder builder;
            builder.setOrderId(nextId++)
                   .setSide(side)
                   .setPrice(price)
                   .setQuantity(qty)
                   .setOrderType(type)
                   .setTimestamp(timeCursor);
            if (display > 0)
                builder.setDisplayQuantity(display);

            auto order = builder.build();
            logging::logger().info("→ new order");
            order->print();
            book.addOrder(std::move(order));
            book.printBook();
            timeCursor += std::chrono::nanoseconds(1);
            return nextId - 1;
        };

        logging::logger().info("\nSeeding resting limit liquidity");
        submitOrder(Side::BUY, 1000, 8, OrderType::LIMIT);
        submitOrder(Side::BUY, 995, 6, OrderType::LIMIT);
        submitOrder(Side::SELL, 1005, 7, OrderType::LIMIT);
        submitOrder(Side::SELL, 1010, 5, OrderType::LIMIT);

        logging::logger().info("\nMarket order sweeps asks");
        submitOrder(Side::BUY, 0, 12, OrderType::MARKET);

        logging::logger().info("\nRebuilding asks for IOC demo");
        submitOrder(Side::SELL, 1004, 4, OrderType::LIMIT);
        submitOrder(Side::SELL, 1006, 5, OrderType::LIMIT);

        logging::logger().info("\nIOC order matches depth and cancels remainder");
        submitOrder(Side::BUY, 1006, 6, OrderType::IOC);
        logging::logger().info("\nIOC priced away cancels immediately");
        submitOrder(Side::BUY, 1000, 4, OrderType::IOC);

        logging::logger().info("\nPreparing depth for FOK scenario");
        submitOrder(Side::SELL, 1005, 2, OrderType::LIMIT);
        submitOrder(Side::SELL, 1005, 2, OrderType::LIMIT);

        logging::logger().info("\nFOK succeeds when full size is available");
        submitOrder(Side::BUY, 1006, 7, OrderType::FOK);

        logging::logger().info("\nFOK fails when liquidity is insufficient");
        submitOrder(Side::SELL, 1008, 3, OrderType::LIMIT);
        submitOrder(Side::BUY, 1006, 5, OrderType::FOK);

        logging::logger().info("\nIceberg order exposes clips and refreshes");
        submitOrder(Side::SELL, 1002, 12, OrderType::ICEBERG, 4);
        submitOrder(Side::BUY, 1002, 4, OrderType::LIMIT);
        submitOrder(Side::BUY, 1002, 4, OrderType::LIMIT);
        submitOrder(Side::BUY, 1002, 4, OrderType::LIMIT);

        logging::logger().info("\n✏Demonstrating modify & cancel on resting orders");
        const OrderId modTarget = submitOrder(Side::BUY, 998, 6, OrderType::LIMIT);
        logging::logger().info("Modifying order {} -> px=1001 qty=9", modTarget);
        book.modifyOrder(modTarget, 1001, 9);
        book.printBook();

        const OrderId cancelTarget = submitOrder(Side::SELL, 1009, 5, OrderType::LIMIT);
        logging::logger().info("Cancelling order {}", cancelTarget);
        if (book.cancelOrder(cancelTarget))
            logging::logger().info("Order {} cancelled", cancelTarget);
        else
            logging::logger().warn("Failed to cancel order {}", cancelTarget);
        book.printBook();

        logging::logger().info("\n📘 Final book state:");
        book.printBook();
    } catch (const std::exception& ex) {
        logging::logger().error("Unhandled exception: {}", ex.what());
        return 1;
    }

    return 0;
}
