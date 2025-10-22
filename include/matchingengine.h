#pragma once

#include "order.h"
#include "orderbook.h"
#include "utils.h"       // <-- ADDED
#include <string>
// #include <fstream>   // <-- REMOVED
#include <mutex>
#include <chrono>

/**
 * @class MatchingEngine
 * @brief The core of the exchange.
 *
 * (No longer logs to file directly. Pushes trades to a thread-safe queue)
 */
class MatchingEngine {
public:
    /**
     * @brief Constructor.
     * @param trade_queue A reference to the thread-safe queue for logging.
     */
    MatchingEngine(ThreadSafeQueue<Trade>& trade_queue); // <-- CHANGED

    /**
     * @brief Destructor.
     */
    ~MatchingEngine() = default; // <-- CHANGED

    /**
     * @brief Adds a new order to the engine. (Thread-safe)
     */
    void add_order(Order& order);

    /**
     * @brief Prints a snapshot of the current order books. (Thread-safe)
     */
    void print_books() const;

private:
    // --- Private Helper Methods ---

    void process_limit_order(Order& order);
    void process_market_order(Order& order);
    void process_ioc_order(Order& order);
    void match(Order& taker_order);
    void add_to_book(Order& order);

    /**
     * @brief Pushes an executed trade onto the logger queue.
     * This function is now extremely fast and lock-free
     * (the lock is inside the queue).
     */
    void log_trade(const Order& taker_order, const Order& maker_order, double price, uint64_t quantity);

    uint64_t get_current_timestamp_ms() const;

    // --- Member Variables ---

    BidBook m_bids;
    AskBook m_asks;
    
    double m_last_trade_price;

    ThreadSafeQueue<Trade>& m_trade_queue; // <-- ADDED

    // --- Thread Safety ---
    mutable std::mutex m_book_mutex; // Still protects m_bids and m_asks

    // --- REMOVED ---
    // std::ofstream m_trade_log_file;
    // std::mutex m_log_mutex;
    // bool m_json_started;
};