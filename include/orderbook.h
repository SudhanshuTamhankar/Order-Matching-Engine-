#pragma once

#include "order.h"
#include <map>
#include <deque>
#include <functional> // For std::less, std::greater
#include <iostream>
#include <iomanip>

/**
 * @class OrderBook
 * @brief A templated order book for one side (Bids or Asks).
 *
 * It uses a std::map to store price levels, sorted by the
 * provided Comparator. Each price level contains a std::deque
 * of orders, enforcing FIFO (time priority).
 *
 * @tparam Comparator The comparison function for sorting prices
 * (e.g., std::greater for Bids, std::less for Asks).
 */
template<typename Comparator = std::less<double>>
class OrderBook {
public:
    // Type aliases for clarity
    using PriceLevel = std::deque<Order>;
    using OrderMap = std::map<double, PriceLevel, Comparator>;

    /**
     * @brief Default constructor.
     */
    OrderBook() = default;

    /**
     * @brief Adds a LIMIT order to the book.
     * Assumes the order is a LIMIT order, as MARKET/IOC
     * orders are handled by the MatchingEngine directly.
     * @param order The order to add.
     */
    void add_order(const Order& order);

    /**
     * @brief Removes an order from the book (e.g., cancellation).
     *
     * This is an O(log N + M) operation, where N is the number of
     * price levels and M is the number of orders at that price level.
     * For a true HFT system, a separate map<order_id, iterator>
     * would be used for O(log N) cancellation.
     *
     * @param order_id The ID of the order to remove.
     * @param price The price of the order (for fast lookup).
     */
    void remove_order(uint64_t order_id, double price);

    /**
     * @brief Gets an iterator to the best price level in the book.
     * For Bids (std::greater), this is the highest price.
     * For Asks (std::less), this is the lowest price.
     * @return A non-const iterator to the best price level.
     */
    typename OrderMap::iterator get_best_price_level() {
        return m_orders.begin();
    }

    /**
     * @brief Gets a const iterator to the best price level.
     */
    typename OrderMap::const_iterator get_best_price_level() const {
        return m_orders.cbegin();
    }

    /**
     * @brief Gets an iterator to the end of the order map.
     */
    typename OrderMap::iterator end() { return m_orders.end(); }

    /**
     * @brief Gets a const iterator to the end of the order map.
     */
    typename OrderMap::const_iterator end() const { return m_orders.cend(); }


    /**
     * @brief Checks if the order book is empty.
     * @return true if empty, false otherwise.
     */
    bool is_empty() const { return m_orders.empty(); }

    /**
     * @brief Prints a snapshot of the order book (for debugging).
     * @param max_levels The maximum number of price levels to print.
     */
    void print_book(int max_levels = 5) const;

    // ... (inside public: section of OrderBook class)

    /**
     * @brief Removes the best price level from the book.
     * This is called by the MatchingEngine when a level is emptied.
     */
    void remove_best_price_level() {
        if (!m_orders.empty()) {
            m_orders.erase(m_orders.begin());
        }
    }

    // ... (rest of the class)

private:
    OrderMap m_orders; // The main data structure
};

// --- Alias definitions for the Matching Engine ---

/**
 * @typedef BidBook
 * @brief An OrderBook sorted for Bids (highest price first).
 */
using BidBook = OrderBook<std::greater<double>>;

/**
 * @typedef AskBook
 * @brief An OrderBook sorted for Asks (lowest price first).
 */
using AskBook = OrderBook<std::less<double>>;