#include <iostream>
#include <iomanip>
#include <string>
#include <thread>         // For std::thread
#include <fstream>        // For the logger's file I/O
#include <sstream>
#include <filesystem>     // For std::filesystem::create_directories
#include <omp.h>          // For OpenMP

#include "../include/utils.h"
#include "../include/matchingengine.h"
#include "../include/generator.h"

/**
 * @brief The function to be run by the dedicated logger thread.
 *
 * This function loops, pulling trades from the queue and writing
 * them to the JSON file. It runs on a separate core and handles
 * all slow file I/O.
 *
 * @param queue The shared, thread-safe queue.
 * @param log_path The file path to write to.
 */
void logger_function(ThreadSafeQueue<Trade>& queue, const std::string& log_path) {
    std::cout << "Logger thread started. Writing to: " << log_path << std::endl;

    // Create results directory
    std::filesystem::create_directories("results");
    
    std::ofstream log_file(log_path, std::ios::out | std::ios::trunc);
    if (!log_file.is_open()) {
        std::cerr << "Logger Error: Could not open file: " << log_path << std::endl;
        return;
    }

    log_file << "[" << std::endl; // Start JSON array
    
    bool json_started = false;
    Trade trade(0,0,0,0,0,OrderSide::BUY); // Dummy trade object to pop into

    // --- Consumer Loop ---
    // This loop will run as long as wait_and_pop returns true.
    // It will return false only when the queue is shut down AND empty.
    while (queue.wait_and_pop(trade)) {
        
        // Add a comma if this is not the first entry
        if (json_started) {
            log_file << ",";
        } else {
            json_started = true;
        }
        
        std::string taker_side = (trade.taker_side == OrderSide::BUY) ? "BUY" : "SELL";
        
        // Format the JSON object
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "\n  {";
        ss << "\"timestamp\": " << trade.timestamp << ", ";
        ss << "\"price\": " << trade.price << ", ";
        ss << "\"quantity\": " << trade.quantity << ", ";
        ss << "\"taker_side\": \"" << taker_side << "\", ";
        ss << "\"taker_id\": " << trade.taker_order_id << ", ";
        ss << "\"maker_id\": " << trade.maker_order_id;
        ss << "}";

        log_file << ss.str();
    }
    
    // Loop finished (queue is empty and shutdown)
    log_file << "\n]" << std::endl; // End JSON array
    log_file.close();
    
    std::cout << "Logger thread finished." << std::endl;
}


/**
 * @brief The main entry point for the application.
 */
int main() {
    // --- 1. Simulation Parameters ---
    const uint64_t TOTAL_ORDERS = 1'000'000; // Let's try 1 Million!
    const std::string LOG_FILE = "results/trades.json";
    const int NUM_THREADS = omp_get_max_threads();
    
    omp_set_num_threads(NUM_THREADS);

    std::cout << "--- High-Performance Order Matching Engine ---" << std::endl;
    std::cout << "Using " << NUM_THREADS << " OpenMP generator threads." << std::endl;
    std::cout << "Simulating " << TOTAL_ORDERS << " orders..." << std::endl;
    
    // --- 2. Initialization ---
    
    // Create the shared queue
    ThreadSafeQueue<Trade> trade_queue;
    
    // Create the engine, passing it the queue
    MatchingEngine engine(trade_queue);
    
    // Create the generator, passing it the engine
    OrderGenerator generator(engine);
    
    // --- 3. Launch Logger Thread ---
    // We launch the logger thread. It will immediately start,
    // find the queue empty, and go to sleep (block)
    // on queue.wait_and_pop().
    std::thread logger(logger_function, std::ref(trade_queue), LOG_FILE);
    
    // --- 4. Launch Generator Threads (Producers) ---
    // This call is blocking. It will use OpenMP to spawn
    // N threads, which all hammer the engine.
    generator.generate_orders(TOTAL_ORDERS);

    std::cout << "Generator finished." << std::endl;

    // --- 5. Graceful Shutdown ---
    
    // (A) Signal the logger queue that no more items are coming
    std::cout << "Signaling logger to shut down..." << std::endl;
    trade_queue.shutdown();
    
    // (B) Wait for the logger thread to finish processing
    // any remaining items in the queue and exit.
    logger.join();

    // --- 6. Print Results ---
    double time_ms = generator.get_time_taken_ms();
    double time_s = time_ms / 1000.0;
    double throughput = static_cast<double>(TOTAL_ORDERS) / time_s;

    std::cout << "\n--- Performance Results ---" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total orders processed: " << TOTAL_ORDERS << std::endl;
    std::cout << "Total time taken:     " << time_s << " seconds (" << time_ms << " ms)" << std::endl;
    std::cout << "Throughput:           " << throughput << " orders/sec" << std::endl;
    
    return 0;
}