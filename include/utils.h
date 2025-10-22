#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "order.h"

/**
 * @struct Trade
 * (This struct is unchanged)
 */
struct Trade {
    uint64_t timestamp;
    double price;
    uint64_t quantity;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    OrderSide taker_side;

    Trade(uint64_t ts, double p, uint64_t qty, uint64_t maker_id, uint64_t taker_id, OrderSide side)
        : timestamp(ts),
          price(p),
          quantity(qty),
          maker_order_id(maker_id),
          taker_order_id(taker_id),
          taker_side(side) {}
};


/**
 * @class ThreadSafeQueue
 * @brief A thread-safe, blocking, producer-consumer queue
 * with graceful shutdown support.
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() : m_shutdown(false) {} // <-- CHANGED
    
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    /**
     * @brief Pushes an item onto the queue and notifies a waiting thread.
     */
    void push(T item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Don't push if the queue is shutting down
        if (m_shutdown) {
            return;
        }

        m_queue.push(std::move(item));
        m_cond.notify_one();
    }

    /**
     * @brief Blocks until an item is available, then pops it.
     * @param item A reference to store the popped item.
     * @return true if an item was popped,
     * false if the queue was shut down and is empty.
     */
    bool wait_and_pop(T& item) { // <-- CHANGED
        std::unique_lock<std::mutex> lock(m_mutex);

        // Wait until queue is not empty OR shutdown is requested
        m_cond.wait(lock, [this]{ 
            return !m_queue.empty() || m_shutdown; 
        });

        // If we woke up because of shutdown AND the queue is empty,
        // stop processing.
        if (m_shutdown && m_queue.empty()) {
            return false;
        }

        // We have an item
        item = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    /**
     * @brief Signals the queue to shut down.
     * All waiting threads will be woken up.
     */
    void shutdown() { // <-- NEW
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
        m_cond.notify_all(); // Wake up all waiting threads
    }

    bool is_empty() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_shutdown; // <-- NEW
};