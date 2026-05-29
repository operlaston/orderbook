#pragma once

#include <Using.h>

class Trade {
private:
  OrderId m_bidId;
  OrderId m_askId;
  Price m_price;
  Quantity m_quantity;
  Timestamp m_timestamp;

public:
  Trade(OrderId bidId, OrderId askId, Price price, Quantity quantity,
        Timestamp timestamp)
      : m_bidId(bidId), m_askId(askId), m_price(price), m_quantity(quantity),
        m_timestamp(timestamp) {}
};
