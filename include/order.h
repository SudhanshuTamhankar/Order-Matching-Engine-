#pragma once

#include <cstdint>
#include <string>

using Price = int64_t;

constexpr Price kPriceScale = 10000;

enum class OrderRequestType : uint8_t {
    NEW,
    CANCEL
};

enum class OrderSide : uint8_t {
    BUY,
    SELL
};

enum class OrderType : uint8_t {
    MARKET,
    LIMIT,
    IOC
};

enum class TimeInForce : uint8_t {
    GTC, // Good 'Til Cancelled
    IOC, // Immediate Or Cancel
    FOK  // Fill Or Kill
};

struct DepthLevel {
    Price price = 0;
    uint64_t total_volume = 0;
    uint64_t order_count = 0;
};

Price price_from_double(double value);
double price_to_double(Price value);
std::string format_price(Price value);

class Order {
public:
    Order();
    Order(uint64_t id,
          OrderSide side,
          OrderType type,
          Price price,
          uint64_t quantity,
          uint64_t timestamp,
          uint32_t asset_id = 1,
          uint32_t account_id = 1,
          uint64_t client_order_id = 0);

    static Order make_cancel(uint64_t request_id,
                             uint64_t target_order_id,
                             uint64_t timestamp,
                             uint32_t asset_id = 1,
                             uint32_t account_id = 1);

    void reset_new(uint64_t id,
                   OrderSide side,
                   OrderType type,
                   Price price,
                   uint64_t quantity,
                   uint64_t timestamp,
                   uint32_t asset_id = 1,
                   uint32_t account_id = 1,
                   uint64_t client_order_id = 0);

    void reset_cancel(uint64_t request_id,
                      uint64_t target_order_id,
                      uint64_t timestamp,
                      uint32_t asset_id = 1,
                      uint32_t account_id = 1);

    uint64_t get_id() const { return m_id; }
    uint64_t get_cancel_target_id() const { return m_cancel_target_id; }
    OrderRequestType get_request_type() const { return m_request_type; }
    OrderSide get_side() const { return m_side; }
    OrderType get_type() const { return m_type; }
    TimeInForce get_tif() const { return m_tif; }
    Price get_price() const { return m_price; }
    uint64_t get_quantity() const { return m_quantity; }
    uint64_t get_timestamp() const { return m_timestamp; }
    uint32_t get_asset_id() const { return m_asset_id; }
    uint32_t get_account_id() const { return m_account_id; }
    uint64_t get_client_order_id() const { return m_client_order_id; }

    void set_quantity(uint64_t new_quantity) { m_quantity = new_quantity; }
    void set_tif(TimeInForce tif) { m_tif = tif; }

    bool is_cancel_request() const { return m_request_type == OrderRequestType::CANCEL; }
    bool is_filled() const { return m_quantity == 0; }

    std::string to_string() const;

private:
    uint64_t m_id;
    uint64_t m_cancel_target_id;
    uint64_t m_client_order_id;
    Price m_price;
    uint64_t m_quantity;
    uint64_t m_timestamp;
    uint32_t m_asset_id;
    uint32_t m_account_id;
    OrderRequestType m_request_type;
    OrderSide m_side;
    OrderType m_type;
    TimeInForce m_tif;
};
