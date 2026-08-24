#include "../include/orderbook.h"

#include <iostream>

using namespace std;

template<typename Comparator>
typename OrderBook<Comparator>::OrderIterator OrderBook<Comparator>::add_order(Order* order) {
    PriceLevel& level = m_levels[order->get_price()];
    level.orders.push_back(order);
    level.total_volume += order->get_quantity();
    auto order_iterator = level.orders.end();
    --order_iterator;
    return order_iterator;
}

template<typename Comparator>
void OrderBook<Comparator>::remove_order(Price price, OrderIterator order_iterator, uint64_t quantity) {
    auto level_iterator = m_levels.find(price);
    if (level_iterator == m_levels.end()) {
        return;
    }

    PriceLevel& level = level_iterator->second;
    if (level.total_volume >= quantity) {
        level.total_volume -= quantity;
    } else {
        level.total_volume = 0;
    }

    level.orders.erase(order_iterator);
    if (level.orders.empty()) {
        m_levels.erase(level_iterator);
    }
}

template<typename Comparator>
void OrderBook<Comparator>::remove_best_price_level() {
    if (!m_levels.empty()) {
        m_levels.erase(m_levels.begin());
    }
}

template<typename Comparator>
void OrderBook<Comparator>::print_book(int max_levels) const {
    int count = 0;
    for (const auto& entry : m_levels) {
        if (count++ >= max_levels) {
            break;
        }

        cout << "  Price: " << format_price(entry.first)
             << "  Qty: " << entry.second.total_volume
             << " (Orders: " << entry.second.orders.size() << ")" << endl;
    }
}

template<typename Comparator>
vector<DepthLevel> OrderBook<Comparator>::snapshot_levels(size_t max_levels) const {
    vector<DepthLevel> snapshot;
    snapshot.reserve(max_levels);

    for (const auto& entry : m_levels) {
        if (snapshot.size() >= max_levels) {
            break;
        }

        DepthLevel level;
        level.price = entry.first;
        level.total_volume = entry.second.total_volume;
        level.order_count = entry.second.orders.size();
        snapshot.push_back(level);
    }

    return snapshot;
}

template class OrderBook<less<Price>>;
template class OrderBook<greater<Price>>;
