#include "../include/matchingengine.h"
#include <iostream>
#include <iomanip>
// #include <sstream> // <-- REMOVED
#include <algorithm> // For std::min

// --- Constructor ---

// The constructor is now trivial. It just stores the
// reference to the trade queue.
MatchingEngine::MatchingEngine(ThreadSafeQueue<Trade>& trade_queue)
    : m_last_trade_price(0.0),
      m_trade_queue(trade_queue) {
    // All file I/O and system calls are GONE.
}

// --- Destructor ---
// The custom destructor implementation is GONE.
// ~MatchingEngine() { ... }


// --- Public Methods ---
// add_order() and print_books() are UNCHANGED.
// (Their implementation is identical to Stage 3)

void MatchingEngine::add_order(Order& order) {
    std::lock_guard<std::mutex> lock(m_book_mutex);
    switch (order.get_type()) {
        case OrderType::LIMIT:
            process_limit_order(order);
            break;
        case OrderType::MARKET:
            process_market_order(order);
            break;
        case OrderType::IOC:
            process_ioc_order(order);
            break;
    }
}

void MatchingEngine::print_books() const {
    std::lock_guard<std::mutex> lock(m_book_mutex);
    std::cout << "\n--- Order Books ---" << std::endl;
    std::cout << "ASKS (SELL):" << std::endl;
    if (m_asks.is_empty()) {
        std::cout << "  (Empty)" << std::endl;
    } else {
        m_asks.print_book(5);
    }
    std::cout << "\nBIDS (BUY):" << std::endl;
    if (m_bids.is_empty()) {
        std::cout << "  (Empty)" << std::endl;
    } else {
        m_bids.print_book(5);
    }
    std::cout << "-------------------" << std::endl;
}

// --- Private Processing Methods ---
// process_limit_order(), process_market_order(),
// process_ioc_order(), and add_to_book() are UNCHANGED.

void MatchingEngine::process_limit_order(Order& order) {
    match(order);
    if (!order.is_filled()) {
        add_to_book(order);
    }
}

void MatchingEngine::process_market_order(Order& order) {
    match(order);
}

void MatchingEngine::process_ioc_order(Order& order) {
    match(order);
}

void MatchingEngine::add_to_book(Order& order) {
    if (order.get_side() == OrderSide::BUY) {
        m_bids.add_order(order);
    } else {
        m_asks.add_order(order);
    }
}

// --- Core Matching Logic ---
// match() is UNCHANGED.
void MatchingEngine::match(Order& taker_order) {
    if (taker_order.is_filled()) {
        return;
    }

    if (taker_order.get_side() == OrderSide::BUY) {
        while (taker_order.get_quantity() > 0 && !m_asks.is_empty()) {
            auto best_ask_level_it = m_asks.get_best_price_level();
            double best_ask_price = best_ask_level_it->first;

            if (taker_order.get_type() == OrderType::MARKET || taker_order.get_price() >= best_ask_price) {
                auto& maker_orders = best_ask_level_it->second;
                while (!maker_orders.empty() && taker_order.get_quantity() > 0) {
                    Order& maker_order = maker_orders.front();
                    uint64_t trade_quantity = std::min(taker_order.get_quantity(), maker_order.get_quantity());
                    if (trade_quantity == 0) break; 
                    log_trade(taker_order, maker_order, best_ask_price, trade_quantity);
                    taker_order.set_quantity(taker_order.get_quantity() - trade_quantity);
                    maker_order.set_quantity(maker_order.get_quantity() - trade_quantity);
                    if (maker_order.is_filled()) {
                        maker_orders.pop_front();
                    }
                }
                if (maker_orders.empty()) {
                    m_asks.remove_best_price_level();
                }
            } else {
                break;
            }
        }
    } else {
        while (taker_order.get_quantity() > 0 && !m_bids.is_empty()) {
            auto best_bid_level_it = m_bids.get_best_price_level();
            double best_bid_price = best_bid_level_it->first;

            if (taker_order.get_type() == OrderType::MARKET || taker_order.get_price() <= best_bid_price) {
                auto& maker_orders = best_bid_level_it->second;
                while (!maker_orders.empty() && taker_order.get_quantity() > 0) {
                    Order& maker_order = maker_orders.front();
                    uint64_t trade_quantity = std::min(taker_order.get_quantity(), maker_order.get_quantity());
                    if (trade_quantity == 0) break;
                    log_trade(taker_order, maker_order, best_bid_price, trade_quantity);
                    taker_order.set_quantity(taker_order.get_quantity() - trade_quantity);
                    maker_order.set_quantity(maker_order.get_quantity() - trade_quantity);
                    if (maker_order.is_filled()) {
                        maker_orders.pop_front();
                    }
                }
                if (maker_orders.empty()) {
                    m_bids.remove_best_price_level();
                }
            } else {
                break;
            }
        }
    }
}

// --- Logging and Utilities ---

/**
 * @brief This is the NEW log_trade.
 * It is now blazing fast. It just creates a Trade struct
 * and pushes it onto the thread-safe queue.
 */
void MatchingEngine::log_trade(const Order& taker_order, const Order& maker_order, double price, uint64_t quantity) {
    // 1. Update LTP
    m_last_trade_price = price;

    // 2. Get timestamp
    uint64_t trade_time = get_current_timestamp_ms();

    // 3. Push the trade data to the queue.
    // The queue handles all its own locking internally.
    // We construct the Trade object in-place.
    m_trade_queue.push(Trade(
        trade_time,
        price,
        quantity,
        maker_order.get_id(),
        taker_order.get_id(),
        taker_order.get_side()
    ));
}

// get_current_timestamp_ms() is UNCHANGED.
uint64_t MatchingEngine::get_current_timestamp_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}