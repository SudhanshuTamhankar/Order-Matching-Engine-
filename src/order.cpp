#include "../include/order.h"
#include <sstream>   // For std::stringstream
#include <iomanip>   // For std::setprecision

// Constructor implementation
Order::Order(uint64_t id, OrderSide side, OrderType type, double price, uint64_t quantity, uint64_t timestamp)
    : m_id(id),
      m_side(side),
      m_type(type),
      m_price(price),
      m_quantity(quantity),
      m_timestamp(timestamp) {
    // For MARKET orders, price is often set to 0 or ignored,
    // as they match at the best available price.
    // For simplicity, we just store the price given.
    // A MARKET buy might have a high price, and a MARKET sell a low (or 0) price.
    // Our matching logic will handle this.
}

// Utility function to convert enum to string
std::string Order::to_string() const {
    std::stringstream ss;

    // Convert OrderSide to string
    std::string side_str = (m_side == OrderSide::BUY) ? "BUY" : "SELL";

    // Convert OrderType to string
    std::string type_str;
    switch (m_type) {
        case OrderType::MARKET:
            type_str = "MARKET";
            break;
        case OrderType::LIMIT:
            type_str = "LIMIT";
            break;
        case OrderType::IOC:
            type_str = "IOC";
            break;
    }

    ss << "Order[ID=" << m_id
       << ", Side=" << side_str
       << ", Type=" << type_str
       << ", Qty=" << m_quantity
       << ", Price=" << std::fixed << std::setprecision(2) << m_price
       << ", TS=" << m_timestamp << "]";

    return ss.str();
}