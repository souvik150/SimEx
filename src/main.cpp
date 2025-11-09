//
// Created by souvik on 11/8/25.
//
#include <iostream>
#include <chrono>

#include "core/OrderBook.h"
#include "core/OrderBuilder.h"

int main() {
    try {
        OrderBook book;

        auto now = std::chrono::high_resolution_clock::now();

        // ──────────────────────────────────────────────
        // 1️⃣ Add first BUY order
        // ──────────────────────────────────────────────
        auto buy1 = OrderBuilder()
            .setOrderId(1)
            .setSide(Side::BUY)
            .setPrice(1000)
            .setQuantity(10)
            .setTimestamp(now)
            .build();

        if (!buy1) {
            std::cerr << "Failed to build BUY order 1\n";
            return 1;
        }
        book.addOrder(std::move(buy1));
        std::cout<<"added order 1"<<std::endl;

        // ──────────────────────────────────────────────
        // 2️⃣ Add first SELL order (won’t match)
        // ──────────────────────────────────────────────
        auto sell1 = OrderBuilder()
            .setOrderId(2)
            .setSide(Side::SELL)
            .setPrice(1050)
            .setQuantity(5)
            .setTimestamp(now)
            .build();
        if (!sell1) {
            std::cerr << "Failed to build SELL order 2\n";
            return 1;
        }
        book.addOrder(std::move(sell1));
        std::cout<<"added order 2"<<std::endl;

        std::cout << "\n📘 Initial Book:\n";
        book.printBook();

        // ──────────────────────────────────────────────
        // 3️⃣ Add another BUY crossing the SELL (match)
        // ──────────────────────────────────────────────
        auto buy2 = OrderBuilder()
            .setOrderId(3)
            .setSide(Side::BUY)
            .setPrice(1060)
            .setQuantity(3)
            .setTimestamp(std::chrono::high_resolution_clock::now())
            .build();

        if (!buy2) {
            std::cerr << "Failed to build BUY order 3\n";
            return 1;
        }
        book.addOrder(std::move(buy2));

        std::cout << "\n📈 After Matching:\n";
        book.printBook();

        // ──────────────────────────────────────────────
        // 4️⃣ Modify existing BUY (increase price)
        // ──────────────────────────────────────────────
        std::cout << "\n✏️  Modifying Order 1 price to 107.0\n";
        book.modifyOrder(1, 1070, 10);


        std::cout << "\n📊 After Modify:\n";
        book.printBook();

        // ──────────────────────────────────────────────
        // 5️⃣ Cancel an order
        // ──────────────────────────────────────────────
        std::cout << "\n❌ Cancelling Order 2\n";
        if (book.cancelOrder(1))
            std::cout << "Order 2 cancelled successfully.\n";
        else
            std::cout << "Failed to cancel order 2.\n";

        std::cout << "\n📘 Final Book Snapshot:\n";
        book.printBook();
    }
    catch (const std::exception& ex) {
        std::cerr << "Unhandled exception: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
