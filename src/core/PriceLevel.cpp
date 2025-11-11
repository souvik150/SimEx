//
// Created by souvik on 11/8/25.
//

#include "core/PriceLevel.h"

#include <sstream>
#include <stdexcept>

#include "utils/LogMacros.h"

PriceLevel::PriceLevel(MemPool<Node>* pool)
    : node_pool_(pool) {}

PriceLevel::PriceLevel(PriceLevel&& other) noexcept {
    head_ = other.head_;
    tail_ = other.tail_;
    order_map_ = std::move(other.order_map_);
    open_qty_ = other.open_qty_;
    node_pool_ = other.node_pool_;

    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.open_qty_ = 0;
    other.node_pool_ = nullptr;
    other.order_map_.clear();
}

PriceLevel& PriceLevel::operator=(PriceLevel&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    clear();

    head_ = other.head_;
    tail_ = other.tail_;
    order_map_ = std::move(other.order_map_);
    open_qty_ = other.open_qty_;
    node_pool_ = other.node_pool_;

    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.open_qty_ = 0;
    other.node_pool_ = nullptr;
    other.order_map_.clear();

    return *this;
}

PriceLevel::Node* PriceLevel::createNode(std::unique_ptr<Order>&& order) {
    if (!node_pool_)
        throw std::runtime_error("PriceLevel allocator not set");
    return node_pool_->allocate(std::move(order));
}

void PriceLevel::releaseNode(Node* node) {
    if (!node_pool_)
        delete node;
    else
        node_pool_->deallocate(node);
}

void PriceLevel::addOrder(std::unique_ptr<Order>&& order) {
    const uint64_t id = order->orderId();
    const Qty pending = order->pending_quantity();
    Node* node = createNode(std::move(order));
    order_map_[id] = node;
    open_qty_ += pending;

    if (!head_) { head_ = tail_ = node; }
    else {
        tail_->next = node;
        node->prev = tail_;
        tail_ = node;
    }
}

ModifyResult PriceLevel::modifyOrder(OrderId order_id, Price new_price, Qty new_qty) {
    const auto it = order_map_.find(order_id);
    if (it == order_map_.end())
        return NotFound;

    const Node* node = it->second;
    auto& originalOrder = *node->order;


    if (new_price != originalOrder.price())
        return NeedsReinsert;

    if (new_qty > originalOrder.quantity()) {
        return NeedsReinsert;
    }

    if (new_qty < originalOrder.quantity()) {
        if (bool result = originalOrder.modifyQty(new_qty); !result) {
            return Invalid;
        }
        return ModifyInPlace;
    }

    return Invalid;
}

 Order* PriceLevel::headOrder() {
    return head_ ? head_->order.get() : nullptr;
}

std::unique_ptr<Order> PriceLevel::popHead() {
    if (!head_) return nullptr;
    Node* node = head_;
    head_ = node->next;
    if (head_) head_->prev = nullptr;
    else tail_ = nullptr;

    const Qty pending = node->order->pending_quantity();
    decOpenQty(pending);
    auto order = std::move(node->order);
    order_map_.erase(order->orderId());
    releaseNode(node);
    return order;
}

std::unique_ptr<Order> PriceLevel::removeOrder(OrderId order_id) {
    const auto it = order_map_.find(order_id);
    if (it == order_map_.end()) return nullptr;

    Node* node = it->second;
    order_map_.erase(it);
    const Qty pending = node->order->pending_quantity();
    decOpenQty(pending);

    if (node->prev) node->prev->next = node->next;
    else head_ = node->next;
    if (node->next) node->next->prev = node->prev;
    else tail_ = node->prev;

    std::unique_ptr<Order> order = std::move(node->order);
    releaseNode(node);
    return order;
}

bool PriceLevel::empty() const { return !head_; }
Qty PriceLevel::openQty() const { return open_qty_; }

void PriceLevel::print() const {
    std::ostringstream out;
    const Node* n = head_;
    out << "[";
    while (n) {
        out << n->order->orderId() << "(" << n->order->quantity() << ")";
        if (n->next) out << " -> ";
        n = n->next;
    }
    out << "]";
    LOG_INFO("{}", out.str());
}

void PriceLevel::clear() {
    Node* n = head_;
    while (n) {
        Node* tmp = n->next;
        releaseNode(n);
        n = tmp;
    }
    order_map_.clear();
    head_ = tail_ = nullptr;
    open_qty_ = 0;
}

void PriceLevel::addFill(Qty tradeQty) {
    if (auto* o = headOrder()) {
        o->addFill(tradeQty);
        decOpenQty(tradeQty);
    }
}

void PriceLevel::decOpenQty(Qty qty) noexcept {
    if (qty > open_qty_) qty = open_qty_;
    open_qty_ -= qty;
}
