#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <omp.h>

#include "../include/generator.h"
#include "../include/matchingengine.h"
#include "../include/utils.h"

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

using namespace std;

namespace {

void flush_trade_batch(ofstream& log_file, const vector<TradeMatch>& batch, bool& is_first_trade) {
    for (const TradeMatch& trade : batch) {
        if (!is_first_trade) {
            log_file << ",";
        } else {
            is_first_trade = false;
        }

        log_file << "\n  {"
                 << "\"timestamp_ms\": " << trade.timestamp_ms << ", "
                 << "\"asset_id\": " << trade.asset_id << ", "
                 << "\"price\": " << format_price(trade.price) << ", "
                 << "\"quantity\": " << trade.quantity << ", "
                 << "\"taker_side\": \"" << (trade.taker_side == OrderSide::BUY ? "BUY" : "SELL") << "\", "
                 << "\"taker_id\": " << trade.taker_order_id << ", "
                 << "\"maker_id\": " << trade.maker_order_id << ", "
                 << "\"taker_account\": " << trade.taker_account_id << ", "
                 << "\"maker_account\": " << trade.maker_account_id
                 << "}";
    }
}

void logger_function(MatchingEngine& engine, const string& log_path) {
    fs::create_directories("results");

    ofstream log_file(log_path, ios::out | ios::trunc);
    if (!log_file.is_open()) {
        cerr << "Logger Error: Could not open file: " << log_path << endl;
        return;
    }

    log_file << "[" << endl;

    vector<TradeMatch> batch;
    batch.reserve(1024);
    bool is_first_trade = true;

    TradeMatch trade;
    while (engine.wait_for_trade(trade)) {
        batch.push_back(trade);
        if (batch.size() >= 1024) {
            flush_trade_batch(log_file, batch, is_first_trade);
            batch.clear();
        }
    }

    if (!batch.empty()) {
        flush_trade_batch(log_file, batch, is_first_trade);
    }

    log_file << "\n]" << endl;
}

void snapshot_writer_function(MatchingEngine& engine, const string& snapshot_path) {
    fs::create_directories("results");

    ofstream snapshot_file(snapshot_path, ios::out | ios::trunc);
    if (!snapshot_file.is_open()) {
        cerr << "Snapshot Error: Could not open file: " << snapshot_path << endl;
        return;
    }

    BookSnapshot snapshot;
    while (engine.wait_for_snapshot(snapshot)) {
        snapshot_file << "{"
                      << "\"timestamp_ms\": " << snapshot.timestamp_ms << ", "
                      << "\"asset_id\": " << snapshot.asset_id << ", "
                      << "\"last_trade_price\": " << format_price(snapshot.last_trade_price) << ", "
                      << "\"bids\": [";

        for (size_t index = 0; index < snapshot.bid_count; ++index) {
            if (index != 0) {
                snapshot_file << ", ";
            }
            snapshot_file << "{"
                          << "\"price\": " << format_price(snapshot.bids[index].price) << ", "
                          << "\"volume\": " << snapshot.bids[index].total_volume << ", "
                          << "\"orders\": " << snapshot.bids[index].order_count
                          << "}";
        }

        snapshot_file << "], \"asks\": [";
        for (size_t index = 0; index < snapshot.ask_count; ++index) {
            if (index != 0) {
                snapshot_file << ", ";
            }
            snapshot_file << "{"
                          << "\"price\": " << format_price(snapshot.asks[index].price) << ", "
                          << "\"volume\": " << snapshot.asks[index].total_volume << ", "
                          << "\"orders\": " << snapshot.asks[index].order_count
                          << "}";
        }

        snapshot_file << "]}" << endl;
    }
}

} // namespace

int main() {
    const uint64_t total_orders = 1000000;
    const string trade_log_path = "results/trades.json";
    const string snapshot_log_path = "results/book_snapshots.jsonl";
    const int num_threads = omp_get_max_threads() > 0 ? omp_get_max_threads() : 4;
    const size_t total_capacity = static_cast<size_t>(total_orders + 250000);
    const size_t per_thread_capacity = (total_capacity / num_threads) + 10000;

    omp_set_num_threads(num_threads);

    cout << "==========================================================" << endl;
    cout << "   Ultra-Low Latency In-Memory Order Matching Engine     " << endl;
    cout << "==========================================================" << endl;
    cout << "Ingress Workers (OpenMP): " << num_threads << " threads" << endl;
    cout << "Simulating Ingestion:     " << total_orders << " requests" << endl;
    cout << "Partitioned Pool Storage: " << (per_thread_capacity * num_threads) << " slots" << endl;

    PartitionedOrderPool order_pool(num_threads, per_thread_capacity);
    MatchingEngine engine(order_pool);
    OrderGenerator generator(engine, order_pool);

    engine.start();

    thread logger(logger_function, ref(engine), trade_log_path);
    thread snapshot_writer(snapshot_writer_function, ref(engine), snapshot_log_path);

    generator.generate_orders(total_orders);
    engine.stop();

    logger.join();
    snapshot_writer.join();

    double time_ms = generator.get_time_taken_ms();
    double time_s = time_ms / 1000.0;
    double throughput = (time_s > 0.0) ? static_cast<double>(total_orders) / time_s : 0.0;

    cout << "\n------------------- Benchmark Summary --------------------" << endl;
    cout << fixed << setprecision(2);
    cout << "Total Requests Submitted: " << total_orders << endl;
    cout << "Ingress Pre-Rejections:  " << generator.get_rejected_orders() << endl;
    cout << "Engine Processed Orders:  " << engine.processed_orders() << endl;
    cout << "Engine Rejected Orders:   " << engine.rejected_orders() << endl;
    cout << "Total Trades Executed:    " << engine.executed_trades() << endl;
    cout << "Pool Available Slots:     " << order_pool.total_available() << " / " << order_pool.total_capacity() << endl;
    cout << "Total Execution Time:     " << time_s << " s (" << time_ms << " ms)" << endl;
    cout << "Engine Ingress Throughput:" << throughput << " orders/sec" << endl;
    cout << "Last Market Trade Price:  " << format_price(engine.last_trade_price()) << endl;
    cout << "==========================================================" << endl;

    return 0;
}
