#include "../include/generator.h"

#include <chrono>
#include <cmath>
#include <iostream>

using namespace std;

OrderGenerator::OrderGenerator(MatchingEngine& engine, PartitionedOrderPool& order_pool)
    : m_engine(engine),
      m_partitioned_pool(&order_pool),
      m_order_pool(nullptr),
      m_start_order_id(1),
      m_time_taken_ms(0.0),
      m_rejected_orders(0),
      m_price_dist(MEAN_PRICE, PRICE_STD_DEV),
      m_qty_dist(MIN_QTY, MAX_QTY),
      m_side_dist(0, 1),
      m_type_dist(0, 99),
      m_request_dist(0, 99),
      m_account_dist(1001, 1050) {
}

OrderGenerator::OrderGenerator(MatchingEngine& engine, OrderPool& order_pool)
    : m_engine(engine),
      m_partitioned_pool(nullptr),
      m_order_pool(&order_pool),
      m_start_order_id(1),
      m_time_taken_ms(0.0),
      m_rejected_orders(0),
      m_price_dist(MEAN_PRICE, PRICE_STD_DEV),
      m_qty_dist(MIN_QTY, MAX_QTY),
      m_side_dist(0, 1),
      m_type_dist(0, 99),
      m_request_dist(0, 99),
      m_account_dist(1001, 1050) {
}

uint64_t OrderGenerator::get_timestamp_ns() const {
    return chrono::duration_cast<chrono::nanoseconds>(
        chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

void OrderGenerator::populate_random_order(Order* order, uint64_t id, mt19937_64& rng) {
    uint64_t timestamp = get_timestamp_ns();
    uint32_t asset_id = 1; // Primary test asset (e.g. AAPL)
    uint32_t account_id = m_account_dist(rng);
    int request_roll = m_request_dist(rng);

    if (id > 1 && request_roll < 12) {
        uniform_int_distribution<uint64_t> cancel_target_dist(1, id - 1);
        order->reset_cancel(id, cancel_target_dist(rng), timestamp, asset_id, account_id);
        return;
    }

    OrderSide side = (m_side_dist(rng) == 0) ? OrderSide::BUY : OrderSide::SELL;
    uint64_t quantity = m_qty_dist(rng);
    Price price = price_from_double(round(m_price_dist(rng) * 100.0) / 100.0);

    OrderType type = OrderType::LIMIT;
    int type_roll = m_type_dist(rng);
    if (type_roll >= 80 && type_roll < 95) {
        type = OrderType::MARKET;
        price = 0;
    } else if (type_roll >= 95) {
        type = OrderType::IOC;
    }

    order->reset_new(id, side, type, price, quantity, timestamp, asset_id, account_id, id);
}

bool OrderGenerator::validate_order(const Order& order) const {
    if (order.is_cancel_request()) {
        return order.get_cancel_target_id() != 0;
    }

    if (order.get_quantity() == 0) {
        return false;
    }

    if (order.get_type() != OrderType::MARKET && order.get_price() <= 0) {
        return false;
    }

    return true;
}

void OrderGenerator::generate_orders(uint64_t num_orders) {
    cout << "Starting parallel validation and ingestion of " << num_orders << " orders..." << endl;

    double start_time = omp_get_wtime();
    uint64_t rejected_orders = 0;

    #pragma omp parallel reduction(+:rejected_orders)
    {
        const int tid = omp_get_thread_num();
        uint64_t seed = static_cast<uint64_t>(chrono::high_resolution_clock::now().time_since_epoch().count())
            ^ (0x9e3779b97f4a7c15ULL + static_cast<uint64_t>(tid));
        mt19937_64 thread_rng(seed);

        #pragma omp for schedule(static)
        for (uint64_t index = 0; index < num_orders; ++index) {
            Order* order = nullptr;
            if (m_partitioned_pool != nullptr) {
                order = m_partitioned_pool->acquire(tid);
            } else if (m_order_pool != nullptr) {
                order = m_order_pool->acquire();
            }

            if (order == nullptr) {
                ++rejected_orders;
                continue;
            }

            uint64_t order_id = m_start_order_id + index;
            populate_random_order(order, order_id, thread_rng);

            if (!validate_order(*order)) {
                if (m_partitioned_pool != nullptr) {
                    m_partitioned_pool->release(tid, order);
                } else if (m_order_pool != nullptr) {
                    m_order_pool->release(order);
                }
                ++rejected_orders;
                continue;
            }

            if (!m_engine.submit_order(order)) {
                if (m_partitioned_pool != nullptr) {
                    m_partitioned_pool->release(tid, order);
                } else if (m_order_pool != nullptr) {
                    m_order_pool->release(order);
                }
                ++rejected_orders;
            }
        }
    }

    double end_time = omp_get_wtime();
    m_time_taken_ms = (end_time - start_time) * 1000.0;
    m_rejected_orders = rejected_orders;
    m_start_order_id += num_orders;
}
