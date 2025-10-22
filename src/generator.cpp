#include "../include/generator.h"
#include <iostream>
#include <chrono>

OrderGenerator::OrderGenerator(MatchingEngine& engine)
    : m_engine(engine),
      m_start_order_id(1), // Start order IDs from 1
      m_time_taken_ms(0.0),
      m_price_dist(MEAN_PRICE, PRICE_STD_DEV),
      m_qty_dist(MIN_QTY, MAX_QTY),
      m_side_dist(0, 1), // 0 = BUY, 1 = SELL
      m_type_dist(0, 100) // Percentage-based for order type
{
    // Note: std::random_device is (ideally) a non-deterministic
    // random source used to seed the pseudo-random generator.
}

uint64_t OrderGenerator::get_timestamp_ns() const {
    // Get current time as nanoseconds since epoch
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

Order OrderGenerator::create_random_order(uint64_t id, std::mt19937& rng) {
    // Generate random properties
    OrderSide side = (m_side_dist(rng) == 0) ? OrderSide::BUY : OrderSide::SELL;
    
    // Generate a price with 2 decimal places
    double price = std::round(m_price_dist(rng) * 100.0) / 100.0;
    
    uint64_t quantity = m_qty_dist(rng);
    uint64_t timestamp = get_timestamp_ns();

    // Determine order type (e.g., 80% LIMIT, 15% MARKET, 5% IOC)
    OrderType type;
    int type_roll = m_type_dist(rng);
    
    if (type_roll < 80) {
        type = OrderType::LIMIT;
    } else if (type_roll < 95) {
        type = OrderType::MARKET;
        // Market orders don't have a meaningful price, but for
        // our matching logic, a "worst-case" price helps.
        // Or we can just set to 0. Our engine handles 0.
        price = 0.0; 
    } else {
        type = OrderType::IOC;
    }

    return Order(id, side, type, price, quantity, timestamp);
}

void OrderGenerator::generate_orders(uint64_t num_orders) {
    std::cout << "Starting parallel generation of " << num_orders << " orders..." << std::endl;

    // Use random_device to get a non-deterministic seed
    std::random_device rd;

    // Start wall-clock timer
    double start_time = omp_get_wtime();

    // --- OpenMP Parallel Region ---
    // This block will be executed by multiple threads.
    // We create a new RNG for each thread.
    #pragma omp parallel
    {
        // --- Thread-Local Variables ---
        // Each thread gets its own *independent* random number generator
        // Seeded with a combination of random_device and the thread's unique ID
        unsigned int seed = rd() + omp_get_thread_num();
        std::mt19937 thread_rng(seed);

        // --- Parallel Loop ---
        // The 'for' loop iterations are divided among the threads
        // The 'schedule(static)' clause gives each thread a large, contiguous
        // chunk of work, which is efficient.
        #pragma omp for schedule(static)
        for (uint64_t i = 0; i < num_orders; ++i) {
            // Atomically fetch and increment the global order ID counter
            // to ensure every order ID is unique.
            // Note: This is a C++11 standard, but not all OpenMP
            // environments (like older g++) supported it well inside
            // parallel regions. A safer OpenMP way is:
            
            // We can't use a shared m_start_order_id easily without a lock.
            // A simpler, high-performance way:
            // Calculate a unique ID based on thread ID and loop index.
            // This avoids any locking or atomics in the hot loop.
            // (Assuming num_orders is much larger than num_threads)
            
            // uint64_t order_id = (omp_get_thread_num() * (num_orders / omp_get_num_threads())) + i;
            // The loop index 'i' is already unique per *total* generation.
            // We just need to add the starting offset.
            
            // Ah, wait. The loop is #pragma omp for. The index 'i' is
            // from 0 to num_orders-1. This is already unique!
            // We just need to add our starting offset.
            uint64_t order_id = m_start_order_id + i;
            
            // Create a random order
            Order order = create_random_order(order_id, thread_rng);

            // Submit the order to the engine.
            // This will call MatchingEngine::add_order(),
            // which contains our std::mutex. The threads will
            // compete for this lock.
            m_engine.add_order(order);
        }
    }
    // --- End of Parallel Region ---
    // All threads sync up here.

    // Stop timer
    double end_time = omp_get_wtime();
    m_time_taken_ms = (end_time - start_time) * 1000.0; // Convert to milliseconds

    // Update the starting ID for the next batch
    m_start_order_id += num_orders;
}