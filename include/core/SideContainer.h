#pragma once

#include <functional>
#include <map>
#include <memory_resource>
#include <memory>

#include "datastructures/RBTree.h"
#include "types/AppTypes.h"
#include "core/PriceLevel.h"

class SideContainer {
public:
    virtual ~SideContainer() = default;
    virtual PriceLevel* best() = 0;
    virtual const PriceLevel* best() const = 0;
    virtual PriceLevel* find(Price price) = 0;
    virtual const PriceLevel* find(Price price) const = 0;
    virtual void insert(Price price, PriceLevel&& level) = 0;
    virtual void erase(Price price) = 0;
    virtual bool empty() const = 0;
    virtual void forEach(const std::function<void(const Price&, PriceLevel&)>& fn) = 0;
    virtual void forEachConst(const std::function<void(const Price&, const PriceLevel&)>& fn) const = 0;
};

template <typename Compare>
class RbTreeSide final : public SideContainer {
public:
    PriceLevel* best() override { return tree_.best(); }
    const PriceLevel* best() const override { return tree_.best(); }

    PriceLevel* find(Price price) override { return tree_.find(price); }
    const PriceLevel* find(Price price) const override { return tree_.find(price); }

    void insert(Price price, PriceLevel&& level) override { tree_.insert(price, std::move(level)); }
    void erase(Price price) override { tree_.erase(price); }
    bool empty() const override { return tree_.empty(); }

    void forEach(const std::function<void(const Price&, PriceLevel&)>& fn) override {
        tree_.inOrder([&](const Price& p, PriceLevel& lvl) { fn(p, lvl); });
    }
    void forEachConst(const std::function<void(const Price&, const PriceLevel&)>& fn) const override {
        tree_.inOrder([&](const Price& p, PriceLevel& lvl) { fn(p, lvl); });
    }

private:
    RBTree<Price, PriceLevel, Compare> tree_;
};

template <typename Compare>
class StdMapSide final : public SideContainer {
public:
    PriceLevel* best() override {
        if (map_.empty()) return nullptr;
        return &map_.begin()->second;
    }
    const PriceLevel* best() const override {
        if (map_.empty()) return nullptr;
        return &map_.begin()->second;
    }

    PriceLevel* find(Price price) override {
        auto it = map_.find(price);
        return (it == map_.end()) ? nullptr : &it->second;
    }
    const PriceLevel* find(Price price) const override {
        auto it = map_.find(price);
        return (it == map_.end()) ? nullptr : &it->second;
    }

    void insert(Price price, PriceLevel&& level) override {
        auto [it, inserted] = map_.try_emplace(price, std::move(level));
        if (!inserted) {
            it->second = std::move(level);
        }
    }
    void erase(Price price) override { map_.erase(price); }
    bool empty() const override { return map_.empty(); }

    void forEach(const std::function<void(const Price&, PriceLevel&)>& fn) override {
        for (auto& [price, level] : map_) {
            fn(price, level);
        }
    }
    void forEachConst(const std::function<void(const Price&, const PriceLevel&)>& fn) const override {
        for (const auto& [price, level] : map_) {
            fn(price, level);
        }
    }

private:
    std::pmr::map<Price, PriceLevel, Compare> map_{&memory_};
    std::pmr::monotonic_buffer_resource memory_;
};

inline std::unique_ptr<SideContainer> makeSideContainer(bool use_std_map, Side side) {
    if (use_std_map) {
        if (side == Side::BUY)
            return std::make_unique<StdMapSide<std::greater<Price>>>();
        return std::make_unique<StdMapSide<std::less<Price>>>();
    }
    if (side == Side::BUY)
        return std::make_unique<RbTreeSide<std::greater<Price>>>();
    return std::make_unique<RbTreeSide<std::less<Price>>>();
}
