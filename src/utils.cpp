#include "../include/utils.h"

using namespace std;

OrderPool::OrderPool(size_t max_orders) {
    m_storage.resize(max_orders);
    m_free_list.reserve(max_orders);
    for (Order& order : m_storage) {
        m_free_list.push_back(&order);
    }
}

Order* OrderPool::acquire() {
    lock_guard<mutex> lock(m_mutex);
    if (m_free_list.empty()) {
        return nullptr;
    }

    Order* order = m_free_list.back();
    m_free_list.pop_back();
    return order;
}

void OrderPool::release(Order* order) {
    if (order == nullptr) {
        return;
    }

    lock_guard<mutex> lock(m_mutex);
    m_free_list.push_back(order);
}

size_t OrderPool::available() const {
    lock_guard<mutex> lock(m_mutex);
    return m_free_list.size();
}

PartitionedOrderPool::PartitionedOrderPool(size_t num_threads, size_t per_thread_capacity)
    : m_num_threads(num_threads ? num_threads : 1) {
    m_pools.reserve(m_num_threads);
    for (size_t i = 0; i < m_num_threads; ++i) {
        m_pools.push_back(make_unique<OrderPool>(per_thread_capacity));
    }
}

Order* PartitionedOrderPool::acquire(size_t thread_id) {
    const size_t idx = thread_id % m_num_threads;
    Order* order = m_pools[idx]->acquire();
    if (order != nullptr) {
        return order;
    }

    // Work-stealing fallback across other thread pools if the local slice is exhausted
    for (size_t i = 0; i < m_num_threads; ++i) {
        if (i == idx) continue;
        order = m_pools[i]->acquire();
        if (order != nullptr) {
            return order;
        }
    }
    return nullptr;
}

void PartitionedOrderPool::release(size_t thread_id, Order* order) {
    if (order == nullptr) return;
    const size_t idx = thread_id % m_num_threads;
    m_pools[idx]->release(order);
}

void PartitionedOrderPool::release_any(Order* order) {
    if (order == nullptr) return;
    m_pools[0]->release(order);
}

size_t PartitionedOrderPool::total_capacity() const {
    size_t cap = 0;
    for (const auto& pool : m_pools) {
        cap += pool->capacity();
    }
    return cap;
}

size_t PartitionedOrderPool::total_available() const {
    size_t avail = 0;
    for (const auto& pool : m_pools) {
        avail += pool->available();
    }
    return avail;
}
