#include "../include/matchingengine.h"

#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std;

MatchingEngine::MatchingEngine(PartitionedOrderPool& order_pool,
                               size_t ingress_capacity,
                               size_t trade_buffer_capacity,
                               size_t snapshot_buffer_capacity,
                               chrono::milliseconds snapshot_interval)
    : m_partitioned_pool(&order_pool),
      m_order_pool(nullptr),
      m_ingress_queue(ingress_capacity),
      m_trade_buffer(trade_buffer_capacity),
      m_snapshot_buffer(snapshot_buffer_capacity),
      m_snapshot_interval(snapshot_interval),
      m_running(false),
      m_last_trade_price(0),
      m_processed_orders(0),
      m_rejected_orders(0),
      m_executed_trades(0) {
    m_order_index.reserve(524288);
}

MatchingEngine::MatchingEngine(OrderPool& order_pool,
                               size_t ingress_capacity,
                               size_t trade_buffer_capacity,
                               size_t snapshot_buffer_capacity,
                               chrono::milliseconds snapshot_interval)
    : m_partitioned_pool(nullptr),
      m_order_pool(&order_pool),
      m_ingress_queue(ingress_capacity),
      m_trade_buffer(trade_buffer_capacity),
      m_snapshot_buffer(snapshot_buffer_capacity),
      m_snapshot_interval(snapshot_interval),
      m_running(false),
      m_last_trade_price(0),
      m_processed_orders(0),
      m_rejected_orders(0),
      m_executed_trades(0) {
    m_order_index.reserve(524288);
}

MatchingEngine::~MatchingEngine() {
    stop();
}

void MatchingEngine::start() {
    if (m_running.exchange(true, memory_order_acq_rel)) {
        return;
    }

    m_matching_thread = thread(&MatchingEngine::matching_loop, this);
}

void MatchingEngine::stop() {
    m_ingress_queue.shutdown();

    if (m_matching_thread.joinable()) {
        m_matching_thread.join();
    }

    m_running.store(false, memory_order_release);
    m_trade_buffer.shutdown();
    m_snapshot_buffer.shutdown();
}

bool MatchingEngine::submit_order(Order* order) {
    return m_ingress_queue.push(order);
}

bool MatchingEngine::wait_for_trade(TradeMatch& trade) {
    return m_trade_buffer.wait_and_pop(trade);
}

bool MatchingEngine::wait_for_snapshot(BookSnapshot& snapshot) {
    return m_snapshot_buffer.wait_and_pop(snapshot);
}

void MatchingEngine::print_books() const {
    cout << "\n--- Order Books ---" << endl;
    cout << "ASKS (SELL):" << endl;
    if (m_asks.is_empty()) {
        cout << "  (Empty)" << endl;
    } else {
        m_asks.print_book(5);
    }

    cout << "\nBIDS (BUY):" << endl;
    if (m_bids.is_empty()) {
        cout << "  (Empty)" << endl;
    } else {
        m_bids.print_book(5);
    }
    cout << "-------------------" << endl;
}

void MatchingEngine::matching_loop() {
    Order* order = nullptr;
    auto last_snapshot_time = chrono::steady_clock::now();

    while (m_ingress_queue.wait_and_pop(order)) {
        process_order(order);

        // Lock-free inline periodic depth sampling directly on matching thread
        auto now = chrono::steady_clock::now();
        if (now - last_snapshot_time >= m_snapshot_interval) {
            m_snapshot_buffer.push(collect_snapshot_internal());
            last_snapshot_time = now;
        }
    }

    // Emit terminal snapshot when queue drains
    m_snapshot_buffer.push(collect_snapshot_internal());
}

void MatchingEngine::process_order(Order* order) {
    if (!validate_order(*order)) {
        m_rejected_orders.fetch_add(1, memory_order_relaxed);
        release_order(order);
        return;
    }

    m_processed_orders.fetch_add(1, memory_order_relaxed);

    if (order->is_cancel_request()) {
        process_cancel_order(order);
        return;
    }

    switch (order->get_type()) {
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

bool MatchingEngine::validate_order(const Order& order) const {
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

void MatchingEngine::process_limit_order(Order* order) {
    match(order);
    if (order->is_filled()) {
        release_order(order);
        return;
    }

    add_to_book(order);
}

void MatchingEngine::process_market_order(Order* order) {
    match(order);
    release_order(order);
}

void MatchingEngine::process_ioc_order(Order* order) {
    match(order);
    release_order(order);
}

void MatchingEngine::process_cancel_order(Order* cancel_request) {
    auto indexed_order = m_order_index.find(cancel_request->get_cancel_target_id());
    if (indexed_order != m_order_index.end()) {
        OrderLocation location = indexed_order->second;
        Order* resting_order = *location.order_iterator;
        uint64_t resting_quantity = resting_order->get_quantity();

        if (location.side == OrderSide::BUY) {
            m_bids.remove_order(location.price, location.order_iterator, resting_quantity);
        } else {
            m_asks.remove_order(location.price, location.order_iterator, resting_quantity);
        }

        m_order_index.erase(indexed_order);
        release_order(resting_order);
    }

    release_order(cancel_request);
}

void MatchingEngine::match(Order* taker_order) {
    if (taker_order->is_filled()) {
        return;
    }

    if (taker_order->get_side() == OrderSide::BUY) {
        while (taker_order->get_quantity() > 0 && !m_asks.is_empty()) {
            auto best_level_iterator = m_asks.get_best_price_level();
            Price best_price = best_level_iterator->first;

            if (taker_order->get_type() != OrderType::MARKET && taker_order->get_price() < best_price) {
                break;
            }

            PriceLevel& level = best_level_iterator->second;
            while (!level.orders.empty() && taker_order->get_quantity() > 0) {
                auto maker_iterator = level.orders.begin();
                Order* maker_order = *maker_iterator;
                uint64_t trade_quantity = min(taker_order->get_quantity(), maker_order->get_quantity());

                if (trade_quantity == 0) {
                    break;
                }

                record_trade(*taker_order, *maker_order, best_price, trade_quantity);
                taker_order->set_quantity(taker_order->get_quantity() - trade_quantity);
                maker_order->set_quantity(maker_order->get_quantity() - trade_quantity);
                level.total_volume -= trade_quantity;

                if (maker_order->is_filled()) {
                    level.orders.pop_front();
                    m_order_index.erase(maker_order->get_id());
                    release_order(maker_order);
                }
            }

            if (level.orders.empty()) {
                m_asks.remove_best_price_level();
            } else {
                break;
            }
        }
        return;
    }

    while (taker_order->get_quantity() > 0 && !m_bids.is_empty()) {
        auto best_level_iterator = m_bids.get_best_price_level();
        Price best_price = best_level_iterator->first;

        if (taker_order->get_type() != OrderType::MARKET && taker_order->get_price() > best_price) {
            break;
        }

        PriceLevel& level = best_level_iterator->second;
        while (!level.orders.empty() && taker_order->get_quantity() > 0) {
            auto maker_iterator = level.orders.begin();
            Order* maker_order = *maker_iterator;
            uint64_t trade_quantity = min(taker_order->get_quantity(), maker_order->get_quantity());

            if (trade_quantity == 0) {
                break;
            }

            record_trade(*taker_order, *maker_order, best_price, trade_quantity);
            taker_order->set_quantity(taker_order->get_quantity() - trade_quantity);
            maker_order->set_quantity(maker_order->get_quantity() - trade_quantity);
            level.total_volume -= trade_quantity;

            if (maker_order->is_filled()) {
                level.orders.pop_front();
                m_order_index.erase(maker_order->get_id());
                release_order(maker_order);
            }
        }

        if (level.orders.empty()) {
            m_bids.remove_best_price_level();
        } else {
            break;
        }
    }
}

void MatchingEngine::add_to_book(Order* order) {
    if (order->get_side() == OrderSide::BUY) {
        auto order_iterator = m_bids.add_order(order);
        m_order_index[order->get_id()] = OrderLocation { order->get_side(), order->get_price(), order_iterator };
        return;
    }

    auto order_iterator = m_asks.add_order(order);
    m_order_index[order->get_id()] = OrderLocation { order->get_side(), order->get_price(), order_iterator };
}

void MatchingEngine::release_order(Order* order) {
    if (m_partitioned_pool != nullptr) {
        m_partitioned_pool->release_any(order);
    } else if (m_order_pool != nullptr) {
        m_order_pool->release(order);
    }
}

void MatchingEngine::record_trade(const Order& taker_order, const Order& maker_order, Price price, uint64_t quantity) {
    m_last_trade_price.store(price, memory_order_relaxed);
    m_executed_trades.fetch_add(1, memory_order_relaxed);

    TradeMatch trade;
    trade.timestamp_ms = now_ms();
    trade.price = price;
    trade.quantity = quantity;
    trade.maker_order_id = maker_order.get_id();
    trade.taker_order_id = taker_order.get_id();
    trade.asset_id = taker_order.get_asset_id();
    trade.maker_account_id = maker_order.get_account_id();
    trade.taker_account_id = taker_order.get_account_id();
    trade.taker_side = taker_order.get_side();

    m_trade_buffer.push(trade);
}

BookSnapshot MatchingEngine::collect_snapshot_internal() const {
    BookSnapshot snapshot;
    snapshot.timestamp_ms = now_ms();
    snapshot.last_trade_price = m_last_trade_price.load(memory_order_relaxed);
    snapshot.asset_id = 1;

    vector<DepthLevel> bid_levels = m_bids.snapshot_levels(kSnapshotDepth);
    vector<DepthLevel> ask_levels = m_asks.snapshot_levels(kSnapshotDepth);

    snapshot.bid_count = bid_levels.size();
    snapshot.ask_count = ask_levels.size();

    for (size_t index = 0; index < bid_levels.size(); ++index) {
        snapshot.bids[index] = bid_levels[index];
    }

    for (size_t index = 0; index < ask_levels.size(); ++index) {
        snapshot.asks[index] = ask_levels[index];
    }

    return snapshot;
}

uint64_t MatchingEngine::now_ms() const {
    return chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()
    ).count();
}
