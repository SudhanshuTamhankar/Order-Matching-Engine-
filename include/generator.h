#pragma once

#include <vector>
#include <random>
#include <cstdint>
#include <omp.h> // <-- Include OpenMP header
#include "matchingengine.h"
#include "order.h"

/**
 * @class OrderGenerator
 * @brief Generates a high volume of random orders in parallel using OpenMP.
 *
 * This class is designed to stress-test the MatchingEngine by
 * submitting orders from multiple threads concurrently.
 */
class OrderGenerator {
public:
    /**
     * @brief Constructor.
     * @param engine A reference to the MatchingEngine to feed orders into.
     */
    OrderGenerator(MatchingEngine& engine);

    /**
     * @brief Generates and processes N orders in parallel.
     *
     * This function uses an OpenMP parallel loop. Each thread
     * gets its own random number generator and creates a
     * subset of the total orders, calling engine.add_order()
     * for each one.
     *
     * @param num_orders The total number of orders to generate.
     */
    void generate_orders(uint64_t num_orders);

    /**
     * @brief Returns the wall-clock time taken for the last
     * generate_orders() call.
     * @return Time taken in milliseconds.
     */
    double get_time_taken_ms() const { return m_time_taken_ms; }

private:
    /**
     * @brief Creates a single randomized order.
     * @param id The unique ID for this order.
     * @param rng The random number generator (must be thread-local).
     * @return A new Order object.
     */
    Order create_random_order(uint64_t id, std::mt19937& rng);

    /**
     * @brief Gets a high-resolution timestamp in nanoseconds.
     */
    uint64_t get_timestamp_ns() const;

    // --- Member Variables ---

    MatchingEngine& m_engine;       // Reference to the engine
    uint64_t m_start_order_id;    // To ensure unique order IDs
    double m_time_taken_ms;       // Performance metric

    // --- Random Distributions ---

    // Constants for realistic order generation
    static constexpr double MEAN_PRICE = 100.0;
    static constexpr double PRICE_STD_DEV = 2.0;
    static constexpr int MIN_QTY = 1;
    static constexpr int MAX_QTY = 100;

    std::normal_distribution<double> m_price_dist;
    std::uniform_int_distribution<uint64_t> m_qty_dist;
    std::uniform_int_distribution<int> m_side_dist;
    std::uniform_int_distribution<int> m_type_dist; // For order type mix
};