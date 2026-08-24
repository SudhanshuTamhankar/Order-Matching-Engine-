#pragma once

#include "order.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

struct TradeMatch {
    uint64_t timestamp_ms = 0;
    Price price = 0;
    uint64_t quantity = 0;
    uint64_t maker_order_id = 0;
    uint64_t taker_order_id = 0;
    uint32_t asset_id = 1;
    uint32_t maker_account_id = 1;
    uint32_t taker_account_id = 1;
    OrderSide taker_side = OrderSide::BUY;
};

constexpr size_t kSnapshotDepth = 10;

struct BookSnapshot {
    uint64_t timestamp_ms = 0;
    Price last_trade_price = 0;
    uint32_t asset_id = 1;
    size_t bid_count = 0;
    size_t ask_count = 0;
    std::array<DepthLevel, kSnapshotDepth> bids {};
    std::array<DepthLevel, kSnapshotDepth> asks {};
};

template<typename T>
class BoundedBlockingQueue {
public:
    explicit BoundedBlockingQueue(size_t capacity)
        : m_capacity(capacity),
          m_shutdown(false) {
    }

    BoundedBlockingQueue(const BoundedBlockingQueue&) = delete;
    BoundedBlockingQueue& operator=(const BoundedBlockingQueue&) = delete;

    bool push(T item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_not_full.wait(lock, [this] {
            return m_queue.size() < m_capacity || m_shutdown;
        });

        if (m_shutdown) {
            return false;
        }

        m_queue.push(std::move(item));
        lock.unlock();
        m_not_empty.notify_one();
        return true;
    }

    bool wait_and_pop(T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_not_empty.wait(lock, [this] {
            return !m_queue.empty() || m_shutdown;
        });

        if (m_queue.empty()) {
            return false;
        }

        item = std::move(m_queue.front());
        m_queue.pop();
        lock.unlock();
        m_not_full.notify_one();
        return true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
        m_not_empty.notify_all();
        m_not_full.notify_all();
    }

    bool is_empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    size_t m_capacity;
    mutable std::mutex m_mutex;
    std::condition_variable m_not_empty;
    std::condition_variable m_not_full;
    std::queue<T> m_queue;
    bool m_shutdown;
};

template<typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacity)
        : m_capacity(capacity + 1),
          m_buffer(m_capacity),
          m_shutdown(false),
          m_head(0),
          m_tail(0) {
    }

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    bool push(T item) {
        while (!m_shutdown.load(std::memory_order_acquire)) {
            const size_t head = m_head.load(std::memory_order_relaxed);
            const size_t next = increment(head);
            if (next != m_tail.load(std::memory_order_acquire)) {
                m_buffer[head] = std::move(item);
                m_head.store(next, std::memory_order_release);
                return true;
            }
            std::this_thread::yield();
        }
        return false;
    }

    bool wait_and_pop(T& item) {
        while (true) {
            const size_t tail = m_tail.load(std::memory_order_relaxed);
            if (tail != m_head.load(std::memory_order_acquire)) {
                item = std::move(m_buffer[tail]);
                m_tail.store(increment(tail), std::memory_order_release);
                return true;
            }

            if (m_shutdown.load(std::memory_order_acquire)) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    void shutdown() {
        m_shutdown.store(true, std::memory_order_release);
    }

    bool is_empty() const {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }

private:
    size_t increment(size_t index) const {
        return (index + 1) % m_capacity;
    }

    size_t m_capacity;
    std::vector<T> m_buffer;
    std::atomic<bool> m_shutdown;

    // Cache-line aligned (64 bytes) to eliminate multi-core false sharing
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};

class OrderPool {
public:
    explicit OrderPool(size_t max_orders);

    Order* acquire();
    void release(Order* order);
    size_t capacity() const { return m_storage.size(); }
    size_t available() const;

private:
    mutable std::mutex m_mutex;
    std::vector<Order> m_storage;
    std::vector<Order*> m_free_list;
};

class PartitionedOrderPool {
public:
    PartitionedOrderPool(size_t num_threads, size_t per_thread_capacity);

    Order* acquire(size_t thread_id);
    void release(size_t thread_id, Order* order);
    void release_any(Order* order);

    size_t total_capacity() const;
    size_t total_available() const;

private:
    size_t m_num_threads;
    std::vector<std::unique_ptr<OrderPool>> m_pools;
};
