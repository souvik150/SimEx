//
// Created by souvik on 11/8/25.
//
#include <iostream>
#include <chrono>
#include <random>
#include "core/OrderBook.h"
#include "core/OrderBuilder.h"

int main() {
    try {
        OrderBook book;
        auto now = std::chrono::high_resolution_clock::now();

        // Seed RNG
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> side_dist(0, 1);     // 0 = BUY, 1 = SELL
        std::uniform_int_distribution<int> price_offset(-30, 30);
        std::uniform_int_distribution<int> qty_dist(1, 15);

        constexpr int MID_PRICE = 1050;
        constexpr int NUM_ORDERS = 100;

        std::cout << "🚀 Generating " << NUM_ORDERS << " random orders...\n";

        for (int i = 1; i <= NUM_ORDERS; ++i) {
            bool isSell = side_dist(rng);
            Price price = MID_PRICE + static_cast<Price>(price_offset(rng));
            Qty qty     = static_cast<Qty>(qty_dist(rng));

            auto order = OrderBuilder()
                .setOrderId(static_cast<OrderId>(i))
                .setSide(isSell ? Side::SELL : Side::BUY)
                .setPrice(price)
                .setQuantity(qty)
                .setTimestamp(now + std::chrono::nanoseconds(i * 100))
                .build();

            if (!order) {
                std::cerr << "Failed to build order " << i << "\n";
                continue;
            }
            book.addOrder(std::move(order));
        }

        std::cout << "\n📘 Initial Randomized Book Snapshot:\n";
        book.printBook();

        // ──────────────────────────────────────────────
        std::cout << "\n✏️  Modifying a few random orders...\n";
        book.modifyOrder(5, 1075, 12);
        book.modifyOrder(10, 1040, 8);
        book.modifyOrder(15, 1060, 7);

        std::cout << "\n📊 After Modify:\n";
        book.printBook();

        // ──────────────────────────────────────────────
        std::cout << "\n❌ Cancelling few random orders...\n";
        for (int id : {3, 8, 20, 33, 42}) {
            if (book.cancelOrder(static_cast<OrderId>(id)))
                std::cout << "Order " << id << " cancelled successfully.\n";
            else
                std::cout << "Failed to cancel order " << id << ".\n";
        }

        std::cout << "\n📘 Final Book Snapshot:\n";
        book.printBook();
    }
    catch (const std::exception& ex) {
        std::cerr << "Unhandled exception: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
