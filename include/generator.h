#pragma once

#include "matchingengine.h"
#include "utils.h"

#include <cstdint>
#include <random>

#include <omp.h>

class OrderGenerator {
public:
    OrderGenerator(MatchingEngine& engine, PartitionedOrderPool& order_pool);
    OrderGenerator(MatchingEngine& engine, OrderPool& order_pool);

    void generate_orders(uint64_t num_orders);

    double get_time_taken_ms() const { return m_time_taken_ms; }
    uint64_t get_rejected_orders() const { return m_rejected_orders; }

private:
    void populate_random_order(Order* order, uint64_t id, std::mt19937_64& rng);
    bool validate_order(const Order& order) const;
    uint64_t get_timestamp_ns() const;

    MatchingEngine& m_engine;
    PartitionedOrderPool* m_partitioned_pool = nullptr;
    OrderPool* m_order_pool = nullptr;
    uint64_t m_start_order_id;
    double m_time_taken_ms;
    uint64_t m_rejected_orders;

    static constexpr double MEAN_PRICE = 100.0;
    static constexpr double PRICE_STD_DEV = 2.0;
    static constexpr int MIN_QTY = 1;
    static constexpr int MAX_QTY = 100;

    std::normal_distribution<double> m_price_dist;
    std::uniform_int_distribution<uint64_t> m_qty_dist;
    std::uniform_int_distribution<int> m_side_dist;
    std::uniform_int_distribution<int> m_type_dist;
    std::uniform_int_distribution<int> m_request_dist;
    std::uniform_int_distribution<uint32_t> m_account_dist;
};
