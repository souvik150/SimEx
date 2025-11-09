//
// Created by souvik on 11/8/25.
//

#include "core/PriceLevel.h"

PriceLevel::PriceLevel(PriceLevel&& other) noexcept {
    head_ = other.head_;
    tail_ = other.tail_;
    order_map_ = std::move(other.order_map_);
    open_qty_ = other.open_qty_;

    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.open_qty_ = 0;
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

    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.open_qty_ = 0;
    other.order_map_.clear();

    return *this;
}

void PriceLevel::addOrder(std::unique_ptr<Order>&& order) {
    const uint64_t id = order->orderId();
    const Qty pending = order->pending_quantity();
    const auto node = new Node(std::move(order));
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
    delete node;
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
    delete node;
    return order;
}

bool PriceLevel::empty() const { return !head_; }
Qty PriceLevel::openQty() const { return open_qty_; }

void PriceLevel::print() const {
    const Node* n = head_;
    std::cout << "[";
    while (n) {
        std::cout << n->order->orderId() << "(" << n->order->quantity() << ")";
        if (n->next) std::cout << " -> ";
        n = n->next;
    }
    std::cout << "]";
}

void PriceLevel::clear() {
    const Node* n = head_;
    while (n) {
        const Node* tmp = n->next;
        delete n;
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
