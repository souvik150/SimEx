//
// Created by souvik on 11/8/25.
//

#ifndef SIMEX_PRICELEVEL_H
#define SIMEX_PRICELEVEL_H
#include <memory>
#include <unordered_map>

#include "Order.h"
#include "types/OrderRequest.h"
#include "utils/MemPool.h"

class PriceLevel {
public:
    struct Node {
        std::unique_ptr<Order> order;
        Node* next = nullptr;
        Node* prev = nullptr;
        explicit Node(std::unique_ptr<Order> o)
            : order(std::move(o)) {}
    };

    explicit PriceLevel(MemPool<Node>* pool = nullptr);
    ~PriceLevel() { clear(); }

    PriceLevel(const PriceLevel&) = delete;
    PriceLevel& operator=(const PriceLevel&) = delete;
    PriceLevel(PriceLevel&& other) noexcept;
    PriceLevel& operator=(PriceLevel&& other) noexcept;

    void setAllocator(MemPool<Node>* pool) { node_pool_ = pool; }

    void addOrder(std::unique_ptr<Order>&& order);
    ModifyResult modifyOrder(OrderId order_id, Price new_price, Qty new_qty);

    Order* headOrder();
    std::unique_ptr<Order> popHead();
    std::unique_ptr<Order> removeOrder(OrderId order_id);
    bool empty() const;
    Qty openQty() const;
    void print() const;
    void clear();
    void addFill(Qty tradeQty);
    void decOpenQty(Qty qty) noexcept;

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::unordered_map<uint64_t, Node*> order_map_;
    Qty open_qty_ = 0;
    MemPool<Node>* node_pool_ = nullptr;

    Node* createNode(std::unique_ptr<Order>&& order);
    void releaseNode(Node* node);
};

#endif //SIMEX_PRICELEVEL_H
