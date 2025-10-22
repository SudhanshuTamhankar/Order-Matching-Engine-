#include "../include/orderbook.h"
#include <iostream>
#include <iomanip>
#include <algorithm> // For std::find_if

/**
 * @brief Adds a LIMIT order to the book.
 */
template<typename Comparator>
void OrderBook<Comparator>::add_order(const Order& order) {
    // MARKET and IOC orders are not to be added to the book.
    if (order.get_type() != OrderType::LIMIT) {
        return;
    }

    // Find the price level, or create it if it doesn't exist
    // m_orders[order.get_price()] creates an empty deque if key not present
    PriceLevel& price_level = m_orders[order.get_price()];

    // Add the order to the back of the queue (FIFO - time priority)
    price_level.push_back(order);
}

/**
 * @brief Removes an order from the book (e.g., cancellation).
 */
template<typename Comparator>
void OrderBook<Comparator>::remove_order(uint64_t order_id, double price) {
    // Find the price level
    auto price_level_it = m_orders.find(price);

    if (price_level_it != m_orders.end()) {
        // Price level found, now find the order in the deque
        PriceLevel& price_level = price_level_it->second;

        auto order_it = std::find_if(price_level.begin(), price_level.end(),
                                     [order_id](const Order& o) {
                                         return o.get_id() == order_id;
                                     });

        if (order_it != price_level.end()) {
            // Order found, erase it
            price_level.erase(order_it);

            // If the price level is now empty, remove it from the map
            if (price_level.empty()) {
                m_orders.erase(price_level_it);
            }
        }
    }
}

/**
 * @brief Prints a snapshot of the order book (for debugging).
 */
template<typename Comparator>
void OrderBook<Comparator>::print_book(int max_levels) const {
    int count = 0;

    // --- MODIFIED C++11 COMPATIBLE LOOP ---
    // The original C++17 structured binding: 
    // for (const auto& [price, price_level] : m_orders)
    // is not supported by all g++ versions.
    // We replace it with a C++11-compatible loop.
    for (const auto& pair : m_orders) {
        if (count++ >= max_levels) break;

        // Manually unpack the key and value from the map's pair
        const double& price = pair.first;
        const PriceLevel& price_level = pair.second;

        std::cout << "  Price: " << std::fixed << std::setprecision(2) << price
                  << "  Qty: ";

        // Calculate total quantity at this price level
        uint64_t total_qty = 0;
        for (const auto& order : price_level) {
            total_qty += order.get_quantity();
        }
        std::cout << total_qty << " (Orders: " << price_level.size() << ")" << std::endl;
    }
}

// --- Explicit Template Instantiation ---
// This is necessary because the implementation is in a .cpp file.
// We tell the compiler to generate code for these specific types.

// For AskBook (lowest price first)
template class OrderBook<std::less<double>>;

// For BidBook (highest price first)
template class OrderBook<std::greater<double>>;