#pragma once

#include "order.h"

#include <functional>
#include <list>
#include <map>
#include <vector>

struct PriceLevel {
    using OrderList = std::list<Order*>;

    OrderList orders;
    uint64_t total_volume = 0;
};

template<typename Comparator>
class OrderBook {
public:
    using OrderList = PriceLevel::OrderList;
    using LevelMap = std::map<Price, PriceLevel, Comparator>;
    using LevelIterator = typename LevelMap::iterator;
    using ConstLevelIterator = typename LevelMap::const_iterator;
    using OrderIterator = typename OrderList::iterator;

    OrderIterator add_order(Order* order);
    void remove_order(Price price, OrderIterator order_iterator, uint64_t quantity);

    LevelIterator get_best_price_level() { return m_levels.begin(); }
    ConstLevelIterator get_best_price_level() const { return m_levels.cbegin(); }
    LevelIterator end() { return m_levels.end(); }
    ConstLevelIterator end() const { return m_levels.cend(); }

    bool is_empty() const { return m_levels.empty(); }
    void remove_best_price_level();
    void print_book(int max_levels = 5) const;
    std::vector<DepthLevel> snapshot_levels(size_t max_levels) const;

private:
    LevelMap m_levels;
};

using BidBook = OrderBook<std::greater<Price>>;
using AskBook = OrderBook<std::less<Price>>;
