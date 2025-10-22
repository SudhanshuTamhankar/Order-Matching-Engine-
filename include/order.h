#pragma once

#include <string>
#include <cstdint> // For uint64_t
#include <iostream> // For std::ostream
#include <iomanip>  // For std::setprecision

/**
 * @enum OrderSide
 * @brief Defines the side of an order (BUY or SELL).
 */
enum class OrderSide {
    BUY,
    SELL
};

/**
 * @enum OrderType
 * @brief Defines the type of an order.
 * - MARKET: Execute immediately at the best available price.
 * - LIMIT: Execute at a specified price or better.
 * - IOC: Immediate Or Cancel. Execute any portion possible immediately
 * and cancel the rest.
 */
enum class OrderType {
    MARKET,
    LIMIT,
    IOC
};

/**
 * @class Order
 * @brief Represents a single order in the matching engine.
 *
 * This class is a simple data container (struct-like) for order properties.
 * It's designed to be lightweight and efficiently stored in the order book.
 */
class Order {
public:
    /**
     * @brief Constructor for an Order.
     * @param id Unique identifier for the order.
     * @param side BUY or SELL.
     * @param type MARKET, LIMIT, or IOC.
     * @param price The price for LIMIT orders (0.0 for MARKET orders).
     * @param quantity The number of shares.
     * @param timestamp The time the order was received (e.g., nanoseconds since epoch).
     */
    Order(uint64_t id, OrderSide side, OrderType type, double price, uint64_t quantity, uint64_t timestamp);

    // --- Getters (all const) ---

    uint64_t get_id() const { return m_id; }
    OrderSide get_side() const { return m_side; }
    OrderType get_type() const { return m_type; }
    double get_price() const { return m_price; }
    uint64_t get_quantity() const { return m_quantity; }
    uint64_t get_timestamp() const { return m_timestamp; }

    // --- Setters (Mutators) ---

    /**
     * @brief Updates the quantity of the order (e.g., after a partial fill).
     * @param new_quantity The remaining quantity.
     */
    void set_quantity(uint64_t new_quantity) { m_quantity = new_quantity; }

    // --- Utility Functions ---

    /**
     * @brief Checks if the order is fully filled (quantity is zero).
     */
    bool is_filled() const { return m_quantity == 0; }

    /**
     * @brief Generates a human-readable string representation of the order.
     */
    std::string to_string() const;

private:
    uint64_t m_id;         // Unique order ID
    OrderSide m_side;      // BUY or SELL
    OrderType m_type;      // MARKET, LIMIT, IOC
    double m_price;        // Price (for LIMIT orders)
    uint64_t m_quantity;   // Remaining quantity
    uint64_t m_timestamp;  // Order reception time
};