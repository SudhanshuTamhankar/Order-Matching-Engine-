#pragma once

#include "orderbook.h"
#include "utils.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

class MatchingEngine {
public:
    MatchingEngine(PartitionedOrderPool& order_pool,
                   size_t ingress_capacity = 262144,
                   size_t trade_buffer_capacity = 1048576,
                   size_t snapshot_buffer_capacity = 4096,
                   std::chrono::milliseconds snapshot_interval = std::chrono::milliseconds(100));
    MatchingEngine(OrderPool& order_pool,
                   size_t ingress_capacity = 262144,
                   size_t trade_buffer_capacity = 1048576,
                   size_t snapshot_buffer_capacity = 4096,
                   std::chrono::milliseconds snapshot_interval = std::chrono::milliseconds(100));
    ~MatchingEngine();

    void start();
    void stop();

    bool submit_order(Order* order);
    bool wait_for_trade(TradeMatch& trade);
    bool wait_for_snapshot(BookSnapshot& snapshot);

    void print_books() const;

    uint64_t processed_orders() const { return m_processed_orders.load(std::memory_order_relaxed); }
    uint64_t rejected_orders() const { return m_rejected_orders.load(std::memory_order_relaxed); }
    uint64_t executed_trades() const { return m_executed_trades.load(std::memory_order_relaxed); }
    Price last_trade_price() const { return m_last_trade_price.load(std::memory_order_relaxed); }

private:
    struct OrderLocation {
        OrderSide side = OrderSide::BUY;
        Price price = 0;
        BidBook::OrderIterator order_iterator;
    };

    void matching_loop();
    void process_order(Order* order);
    bool validate_order(const Order& order) const;
    void process_limit_order(Order* order);
    void process_market_order(Order* order);
    void process_ioc_order(Order* order);
    void process_cancel_order(Order* cancel_request);
    void match(Order* taker_order);
    void add_to_book(Order* order);
    void release_order(Order* order);
    void record_trade(const Order& taker_order, const Order& maker_order, Price price, uint64_t quantity);
    BookSnapshot collect_snapshot_internal() const;
    uint64_t now_ms() const;

    PartitionedOrderPool* m_partitioned_pool = nullptr;
    OrderPool* m_order_pool = nullptr;
    BoundedBlockingQueue<Order*> m_ingress_queue;
    SpscRingBuffer<TradeMatch> m_trade_buffer;
    SpscRingBuffer<BookSnapshot> m_snapshot_buffer;
    std::chrono::milliseconds m_snapshot_interval;

    BidBook m_bids;
    AskBook m_asks;
    std::unordered_map<uint64_t, OrderLocation> m_order_index;

    std::atomic<bool> m_running;
    std::thread m_matching_thread;

    std::atomic<Price> m_last_trade_price;
    std::atomic<uint64_t> m_processed_orders;
    std::atomic<uint64_t> m_rejected_orders;
    std::atomic<uint64_t> m_executed_trades;
};
