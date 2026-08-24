#include "../include/order.h"

#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

Price price_from_double(double value) {
    return static_cast<Price>(llround(value * static_cast<double>(kPriceScale)));
}

double price_to_double(Price value) {
    return static_cast<double>(value) / static_cast<double>(kPriceScale);
}

string format_price(Price value) {
    stringstream ss;
    ss << fixed << setprecision(4) << price_to_double(value);
    return ss.str();
}

Order::Order()
    : m_id(0),
      m_cancel_target_id(0),
      m_client_order_id(0),
      m_price(0),
      m_quantity(0),
      m_timestamp(0),
      m_asset_id(1),
      m_account_id(1),
      m_request_type(OrderRequestType::NEW),
      m_side(OrderSide::BUY),
      m_type(OrderType::LIMIT),
      m_tif(TimeInForce::GTC) {
}

Order::Order(uint64_t id,
             OrderSide side,
             OrderType type,
             Price price,
             uint64_t quantity,
             uint64_t timestamp,
             uint32_t asset_id,
             uint32_t account_id,
             uint64_t client_order_id) {
    reset_new(id, side, type, price, quantity, timestamp, asset_id, account_id, client_order_id);
}

Order Order::make_cancel(uint64_t request_id,
                         uint64_t target_order_id,
                         uint64_t timestamp,
                         uint32_t asset_id,
                         uint32_t account_id) {
    Order cancel_request;
    cancel_request.reset_cancel(request_id, target_order_id, timestamp, asset_id, account_id);
    return cancel_request;
}

void Order::reset_new(uint64_t id,
                      OrderSide side,
                      OrderType type,
                      Price price,
                      uint64_t quantity,
                      uint64_t timestamp,
                      uint32_t asset_id,
                      uint32_t account_id,
                      uint64_t client_order_id) {
    m_id = id;
    m_cancel_target_id = 0;
    m_client_order_id = client_order_id;
    m_price = price;
    m_quantity = quantity;
    m_timestamp = timestamp;
    m_asset_id = asset_id;
    m_account_id = account_id;
    m_request_type = OrderRequestType::NEW;
    m_side = side;
    m_type = type;
    m_tif = (type == OrderType::IOC) ? TimeInForce::IOC : TimeInForce::GTC;
}

void Order::reset_cancel(uint64_t request_id,
                         uint64_t target_order_id,
                         uint64_t timestamp,
                         uint32_t asset_id,
                         uint32_t account_id) {
    m_id = request_id;
    m_cancel_target_id = target_order_id;
    m_client_order_id = 0;
    m_price = 0;
    m_quantity = 0;
    m_timestamp = timestamp;
    m_asset_id = asset_id;
    m_account_id = account_id;
    m_request_type = OrderRequestType::CANCEL;
    m_side = OrderSide::BUY;
    m_type = OrderType::LIMIT;
    m_tif = TimeInForce::GTC;
}

string Order::to_string() const {
    stringstream ss;

    if (is_cancel_request()) {
        ss << "CancelRequest[ID=" << m_id
           << ", Target=" << m_cancel_target_id
           << ", Asset=" << m_asset_id
           << ", Account=" << m_account_id
           << ", TS=" << m_timestamp << "]";
        return ss.str();
    }

    const string side = (m_side == OrderSide::BUY) ? "BUY" : "SELL";

    string type;
    switch (m_type) {
        case OrderType::MARKET:
            type = "MARKET";
            break;
        case OrderType::LIMIT:
            type = "LIMIT";
            break;
        case OrderType::IOC:
            type = "IOC";
            break;
    }

    ss << "Order[ID=" << m_id
       << ", Asset=" << m_asset_id
       << ", Account=" << m_account_id
       << ", Side=" << side
       << ", Type=" << type
       << ", Qty=" << m_quantity
       << ", Price=" << format_price(m_price)
       << ", TS=" << m_timestamp << "]";

    return ss.str();
}
